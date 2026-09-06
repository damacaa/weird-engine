#include "weird-renderer/audio/PhysicsAudioEngine.h"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		PhysicsAudioEngine::PhysicsAudioEngine()
		{
			m_activeVoices.reserve(MAX_PHYSICS_VOICES);
		}

		PhysicsAudioEngine::~PhysicsAudioEngine() {}

		void PhysicsAudioEngine::init(uint32_t sampleRate, uint32_t channels)
		{
			m_sampleRate = sampleRate;
			m_channels = channels;
			m_activeVoices.clear();
			m_frictionLevel = 0.0f;
			m_smoothedFriction = 0.0f;
			m_lastRawFriction = 0.0f;
			m_recentImpactEnergy = 0.0f;
		}

		float PhysicsAudioEngine::generateNoiseSample()
		{
			// Xorshift32 PRNG
			m_noiseSeed ^= m_noiseSeed << 13;
			m_noiseSeed ^= m_noiseSeed >> 17;
			m_noiseSeed ^= m_noiseSeed << 5;
			float white = (static_cast<float>(m_noiseSeed) / 2147483648.0f) - 1.0f;

			// Paul Kellet's filter for pink noise approximation (-3dB/octave)
			m_pinkB0 = 0.99765f * m_pinkB0 + white * 0.0990460f;
			m_pinkB1 = 0.96300f * m_pinkB1 + white * 0.2965164f;
			m_pinkB2 = 0.57000f * m_pinkB2 + white * 1.0526913f;
			float pink = m_pinkB0 + m_pinkB1 + m_pinkB2 + white * 0.1848f;
			return std::clamp(pink * 0.22f, -1.0f, 1.0f);
		}

		void PhysicsAudioEngine::setFrictionLevel(float level)
		{
			constexpr float MAX_FRICTION = 1.0f;
			const float normalizedFriction = (std::min)(level / MAX_FRICTION, 1.0f);

			constexpr float MIN_AUDIBLE = 0.001f;
			constexpr float MIN_COMPENSATION = 1.0f / (1.0f - MIN_AUDIBLE);
			const float adjustedAmplitude = (std::max)(normalizedFriction - MIN_AUDIBLE, 0.0f) * MIN_COMPENSATION;

			constexpr float LOW_END_BOOST_EXPONENT = 0.5f;
			constexpr float MAX_AMPLITUDE = 0.75f;
			const float initialAmplitude = MAX_AMPLITUDE * pow(adjustedAmplitude, LOW_END_BOOST_EXPONENT);

			constexpr float COMPRESSION_THRESHOLD = 0.3f;
			constexpr float COMPRESSION_RATIO = 4.0f;
			if (initialAmplitude > COMPRESSION_THRESHOLD)
			{
				float overshoot = initialAmplitude - COMPRESSION_THRESHOLD;
				float compressedOvershoot = overshoot / COMPRESSION_RATIO;
				m_frictionLevel = COMPRESSION_THRESHOLD + compressedOvershoot;
			}
			else
			{
				m_frictionLevel = initialAmplitude;
			}
		}

		float PhysicsAudioEngine::getFrictionLevel() const
		{
			return m_frictionLevel;
		}

		void PhysicsAudioEngine::playSound(const SimpleAudioRequest& request, const vec3& listenerPos,
										   const vec3& listenerForward, const vec3& listenerUp)
		{
			float vol = request.volume * m_volume;
			if (vol <= 0.001f)
				return;

			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = request.frequency > 0.0f ? request.frequency : 20000.0f;

			if (request.spatial)
			{
				bool spatialEnabled = m_spatialEnabled.load(std::memory_order_acquire);
				SpatialAudioResult spatial = SpatialAudioProcessor::process(
					request.position, listenerPos, listenerForward, listenerUp, spatialEnabled);

				vol *= spatial.distanceGain;
				leftGain = spatial.leftGain;
				rightGain = spatial.rightGain;
				filterCutoff = (std::min)(filterCutoff, spatial.filterCutoff);
			}

			if (vol <= 0.001f)
				return;

			float decay = 0.12f * static_cast<float>((std::max)(1, request.beats));
			if (request.intensity > 0.0f && request.instrument == 3)
			{
				decay = 0.03f + 0.08f * std::clamp(request.intensity, 0.0f, 1.0f);
			}

			// Track recent impact energy
			m_recentImpactEnergy = (std::max)(m_recentImpactEnergy, request.intensity);

			playVoice(request.frequency > 0.0f ? request.frequency : 180.0f, (std::min)(1.0f, vol), decay,
					  request.instrument, leftGain, rightGain, filterCutoff);
		}

		void PhysicsAudioEngine::playVoice(float freq, float amp, float decaySec, int instrument, float leftGain,
										   float rightGain, float filterCutoff)
		{
			if (amp <= 0.001f)
				return;

			PhysicsVoice newVoice;
			newVoice.frequency = freq;
			newVoice.amplitude = amp;
			newVoice.decay = (std::max)(0.01f, decaySec);
			newVoice.time = 0.0f;
			newVoice.phase = 0.0f;
			newVoice.finished = false;
			newVoice.instrument = instrument;
			newVoice.leftGain = leftGain;
			newVoice.rightGain = rightGain;
			newVoice.filterCutoff = filterCutoff;
			newVoice.filterState = 0.0f;

			if (m_activeVoices.size() >= MAX_PHYSICS_VOICES)
			{
				// Replace voice with lowest remaining amplitude
				size_t victimIdx = 0;
				float minAmp = 1000.0f;
				for (size_t i = 0; i < m_activeVoices.size(); ++i)
				{
					float curAmp =
						m_activeVoices[i].amplitude * expf(-m_activeVoices[i].time / m_activeVoices[i].decay);
					if (curAmp < minAmp)
					{
						minAmp = curAmp;
						victimIdx = i;
					}
				}
				if (newVoice.amplitude > minAmp * 0.5f)
				{
					m_activeVoices[victimIdx] = newVoice;
				}
				return;
			}

			m_activeVoices.push_back(newVoice);
		}

		void PhysicsAudioEngine::render(float* buffer, uint32_t frameCount, uint32_t channels)
		{
			if (frameCount == 0 || channels != 2)
				return;

			// 1. Smooth continuous friction level
			float oldSmoothed = m_smoothedFriction;
			if (m_frictionLevel > m_smoothedFriction)
			{
				m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * 0.40f; // Fast attack on collision
			}
			else
			{
				m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * 0.08f; // Natural decay
			}

			// Render continuous friction noise
			if (m_smoothedFriction > 0.0001f || oldSmoothed > 0.0001f)
			{
				float oldGain = (std::min)(1.0f, oldSmoothed * 1.10f) * m_volume;
				float newGain = (std::min)(1.0f, m_smoothedFriction * 1.10f) * m_volume;

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					float t = static_cast<float>(i) / static_cast<float>(frameCount);
					float currentGain = oldGain + t * (newGain - oldGain);
					float sample = generateNoiseSample() * currentGain;

					buffer[i * 2 + 0] += sample * 0.7071f;
					buffer[i * 2 + 1] += sample * 0.7071f;
				}
			}

			// 2. Render active physics voices
			const float attackTime = 0.006f;
			const float sampleRateF = static_cast<float>(m_sampleRate);

			for (auto& voice : m_activeVoices)
			{
				if (voice.finished)
					continue;

				const float phaseInc = 2.0f * static_cast<float>(M_PI) * voice.frequency / sampleRateF;
				const float wc =
					2.0f * static_cast<float>(M_PI) * std::clamp(voice.filterCutoff, 20.0f, 20000.0f) / sampleRateF;
				const float filterAlpha = std::clamp(wc / (wc + 1.0f), 0.001f, 1.0f);

				for (uint32_t i = 0; i < frameCount; ++i)
				{
					float env = voice.amplitude * expf(-voice.time / voice.decay);
					if (voice.time < attackTime)
					{
						env *= (voice.time / attackTime);
					}

					float rawSample = 0.0f;
					switch (voice.instrument)
					{
						case 1: // Sawtooth
							rawSample = 1.0f - (voice.phase / static_cast<float>(M_PI));
							break;
						case 2: // Square
							rawSample = (voice.phase < static_cast<float>(M_PI)) ? 0.6f : -0.6f;
							break;
						case 4: // Modal Body Resonance (fundamental + 2nd & 3rd overtone)
							rawSample = 0.68f * sinf(voice.phase) + 0.22f * sinf(voice.phase * 2.0f) +
										0.10f * sinf(voice.phase * 3.0f);
							break;
						case 0: // Sine
							rawSample = sinf(voice.phase);
							break;
						case 3: // Filtered Noise (default for impact/foley)
						default:
							rawSample = generateNoiseSample();
							break;
					}

					// 1-Pole Lowpass Filter
					voice.filterState += filterAlpha * (rawSample - voice.filterState);
					float sample = env * voice.filterState;

					voice.phase += phaseInc;
					if (voice.phase >= 2.0f * static_cast<float>(M_PI))
					{
						voice.phase -= 2.0f * static_cast<float>(M_PI);
					}
					voice.time += 1.0f / sampleRateF;

					buffer[i * 2 + 0] += sample * voice.leftGain;
					buffer[i * 2 + 1] += sample * voice.rightGain;
				}

				if (voice.time > attackTime && voice.amplitude * expf(-voice.time / voice.decay) < 0.001f)
				{
					voice.finished = true;
				}
			}

			// Clean up finished voices
			m_activeVoices.erase(std::remove_if(m_activeVoices.begin(), m_activeVoices.end(),
												[](const PhysicsVoice& v) { return v.finished; }),
								 m_activeVoices.end());
		}

		void PhysicsAudioEngine::reset()
		{
			m_activeVoices.clear();
			m_frictionLevel = 0.0f;
			m_smoothedFriction = 0.0f;
			m_recentImpactEnergy = 0.0f;
		}

		float PhysicsAudioEngine::consumeRecentImpactEnergy()
		{
			float val = m_recentImpactEnergy;
			m_recentImpactEnergy = 0.0f;
			return val;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
