#include "weird-renderer/audio/AudioEngine.h"
#include "weird-engine/Logger.h"
#include "weird-renderer/audio/AudioPresets.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <miniaudio/miniaudio.h>

#include "weird-engine/Input.h"

#include <random>

#define CHANNELS 2
#define SAMPLE_RATE 44100

#ifndef M_PI
#define M_PI 3.14159265359f
#endif // !M_PI

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		constexpr int MAX_ACTIVE_VOICES = 32;

		AudioEngine::AudioEngine()
			: m_mute(false)
		{
		}

		AudioEngine::~AudioEngine() {}

		bool AudioEngine::init(const AudioSettings& settings)
		{
			m_mute = settings.mute;
			m_enableAmbient = settings.enableAmbient;
			// Initialize audio presets registry
			initAudioPresets();
			ma_result result;
			ma_engine_config engineConfig = ma_engine_config_init();
			engineConfig.noDevice = MA_TRUE;
			engineConfig.channels = CHANNELS;
			engineConfig.sampleRate = SAMPLE_RATE;

			result = ma_engine_init(&engineConfig, &m_engine);
			if (result != MA_SUCCESS)
			{
				WeirdEngine::Logger::error("Failed to initialize audio engine");
				return false;
			}

			// Initialize a persistent noise generator for friction
			ma_noise_config noiseConfig = ma_noise_config_init(ma_format_f32, CHANNELS, ma_noise_type_pink,
															   0,	// seed
															   1.0f // amplitude
			);

			ma_result noiseResult = ma_noise_init(&noiseConfig, nullptr, &m_noise);
			if (noiseResult != MA_SUCCESS)
			{
				WeirdEngine::Logger::error("Failed to initialize noise generator");
				return false;
			}

			return true;
		}

		void AudioEngine::close()
		{
			m_audioStream = nullptr;
			ma_noise_uninit(&m_noise, nullptr);
			ma_sound_uninit(&m_sound);
			ma_engine_uninit(&m_engine);
		}

		void AudioEngine::loadSound(const char* filePath)
		{
			ma_result result = ma_sound_init_from_file(&m_engine, filePath, 0, NULL, NULL, &m_sound);
			if (result != MA_SUCCESS)
			{
				ma_engine_uninit(&m_engine);
				throw std::runtime_error("Failed to initialize sound");
			}

			ma_sound_set_looping(&m_sound, MA_TRUE);
			ma_sound_set_volume(&m_sound, 0.0);
			ma_sound_start(&m_sound);
		}

		ma_uint32 AudioEngine::getSampleRate() const
		{
			return ma_engine_get_sample_rate(&m_engine);
		}

		ma_uint8 AudioEngine::getChannels() const
		{
			return ma_engine_get_channels(&m_engine);
		}

		// Standard Reference: A4 = 440Hz
		const float A4_FREQ = 440.0f;
		const int A4_MIDI = 69;

		// Helper: Convert Frequency to MIDI Note Number (e.g., 60.0 is Middle C)
		float freqToMidi(float freq)
		{
			return 69.0f + 12.0f * std::log2(freq / 440.0f);
		}

		// Helper: Convert MIDI Note Number back to Frequency
		float midiToFreq(float midiNote)
		{
			return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
		}

		/**
		 * rounds the input frequency to the nearest note in the C Major Pentatonic scale.
		 * Pentatonic scales are famous because almost any combination of notes
		 * within them sounds "pleasant" together.
		 */
		float getPleasantFrequency(float inputFreq)
		{
			// 1. Convert input frequency to a continuous MIDI note value
			float continuousNote = freqToMidi(inputFreq);

			// 2. Round to nearest integer (nearest semitone)
			int roundedNote = static_cast<int>(std::round(continuousNote));
			// 3. Define a "Safe" Scale (C Major Pentatonic: C, D, E, G, A)
			// Notes relative to C: 0, 2, 4, 7, 9
			std::vector<int> allowedIntervals = {0, 2, 4, 7, 9};

			int closestNote = roundedNote;
			float minDistance = 100.0f;

			// Search neighboring notes to find the closest allowed note
			for (int offset = -2; offset <= 2; ++offset)
			{
				int candidate = roundedNote + offset;
				int interval = candidate % 12;
				if (interval < 0)
					interval += 12;

				for (int allowed : allowedIntervals)
				{
					if (interval == allowed)
					{
						float dist = std::abs(static_cast<float>(candidate) - continuousNote);
						if (dist < minDistance)
						{
							minDistance = dist;
							closestNote = candidate;
						}
					}
				}
			}

			// 4. Convert back to frequency
			return midiToFreq(static_cast<float>(closestNote));
		}

		void AudioEngine::listen(Scene& scene)
		{
			if (m_mute || !m_audioStream)
				return;

			float frictionValue = scene.getFrictionSound();
			float frictionDiff = frictionValue - m_lastRawFriction;
			m_lastRawFriction = frictionValue;

			if (frictionDiff > 0.15f)
			{
				// Physical collision impact noise rumble spike
				float spikeGain = (std::min)(0.85f, frictionDiff * 0.9f);
				playVoice(75.0f, spikeGain, 0.08f, InstrumentType::Noise, 0.7071f, 0.7071f, 1500.0f);
			}

			setFrictionLevel(frictionValue);

			AudioModule* module = scene.getAudioModule();
			if (module)
			{
				module->setFrictionLevel(frictionValue);
			}

			auto& audioQueue = scene.getAudioQueue();
			auto& camera = scene.getCamera();
			auto cameraPosition = camera.position;

			// Drain and process explicit playSound audio requests from scene
			SimpleAudioRequest req;
			while (audioQueue.pop(req))
			{
				float vol = req.volume;
				float freq = req.frequency;
				float leftGain = 0.7071f;
				float rightGain = 0.7071f;
				float filterCutoff = 20000.0f;

				InstrumentType inst = InstrumentType::Sine;
				if (req.instrument == 1)
					inst = InstrumentType::Sawtooth;
				else if (req.instrument == 2)
					inst = InstrumentType::Square;
				else if (req.instrument == 3)
					inst = InstrumentType::Noise;
				else if (req.instrument == 4)
					inst = InstrumentType::Polyphonic;

				// For non-noise sounds, harmonically quantize to active scale / chord
				if (inst != InstrumentType::Noise)
				{
					if (module)
					{
						freq = module->quantizeToHarmonics(freq, req.intensity);
					}
					else if (freq <= 0.0f)
					{
						freq = 440.0f;
					}

					// Apply gentle warmth cutoff so UI clicks and input tones blend seamlessly with music
					if (inst == InstrumentType::Sine || inst == InstrumentType::Polyphonic)
					{
						filterCutoff = 5500.0f;
					}
					else if (inst == InstrumentType::Square)
					{
						filterCutoff = 1500.0f; // Warm lowpass on square wave (e.g. error buzz)
					}
				}
				else if (freq <= 0.0f)
				{
					freq = 300.0f;
				}

				if (req.spatial)
				{
					vec3 toSource = req.position - cameraPosition;
					float dist = glm::length(toSource);

					// Distance attenuation: smooth inverse distance law preserving clarity at reference distance
					const float DIST_REF = 12.0f;
					float distanceFactor = DIST_REF / (DIST_REF + dist * 0.45f);
					vol *= distanceFactor;

					// High-frequency air absorption / distance damping
					float distCutoff = 20000.0f / (1.0f + 0.015f * dist);

					// Compute camera basis vectors for stereo panning
					vec3 forward = glm::length2(camera.orientation) > 0.001f ? glm::normalize(camera.orientation)
																			 : vec3(0.0f, 0.0f, -1.0f);
					vec3 up = glm::length2(camera.up) > 0.001f ? glm::normalize(camera.up) : vec3(0.0f, 1.0f, 0.0f);
					vec3 right = glm::cross(forward, up);
					if (glm::length2(right) > 0.001f)
						right = glm::normalize(right);
					else
						right = vec3(1.0f, 0.0f, 0.0f);

					float rightDist = glm::dot(toSource, right);
					float forwardDist = glm::dot(toSource, forward);

					// Azimuth angle relative to camera view
					float azimuth = std::atan2(rightDist, (std::max)(1.0f, std::abs(forwardDist)));
					float halfFovRad = glm::radians(camera.fov > 1.0f ? camera.fov : 45.0f) * 0.5f;
					float pan = std::clamp(azimuth / halfFovRad, -1.0f, 1.0f);

					// Equal-power stereo panning
					float panAngle = (pan + 1.0f) * (static_cast<float>(M_PI) * 0.25f);
					leftGain = std::cos(panAngle);
					rightGain = std::sin(panAngle);

					if (inst == InstrumentType::Noise)
					{
						filterCutoff = (std::min)(freq, distCutoff);
					}
					else
					{
						filterCutoff = (std::min)(filterCutoff, distCutoff);
					}
				}
				else
				{
					if (inst == InstrumentType::Noise && freq > 0.0f)
					{
						filterCutoff = freq;
					}
				}

				if (vol > 0.002f)
				{
					float decay = 0.12f * static_cast<float>((std::max)(1, req.beats));
					if (req.intensity > 0.0f && inst == InstrumentType::Noise)
					{
						decay = 0.03f + 0.08f * std::clamp(req.intensity, 0.0f, 1.0f);
					}
					playVoice(freq, (std::min)(1.0f, vol), decay, inst, leftGain, rightGain, filterCutoff);

					if (module)
					{
						module->surge(vol * 0.35f);
					}
				}
			}

			// --- Generate and push audio frames to SDL stream ---
			constexpr int TARGET_BUFFER_BYTES = (SAMPLE_RATE * CHANNELS * sizeof(float) * 8) / 100;
			int queuedBytes = SDL_GetAudioStreamQueued(m_audioStream);
			if (queuedBytes < TARGET_BUFFER_BYTES)
			{
				int bytesToGenerate = TARGET_BUFFER_BYTES - queuedBytes;
				ma_uint32 framesToWrite = static_cast<ma_uint32>(bytesToGenerate / (CHANNELS * sizeof(float)));
				if (framesToWrite > 4096)
					framesToWrite = 4096;

				if (framesToWrite > 0)
				{
					std::vector<float> mix(framesToWrite * CHANNELS, 0.0f);
					std::vector<float> temp(framesToWrite * CHANNELS, 0.0f);

					// 1. Miniaudio background music
					ma_engine_read_pcm_frames(&m_engine, mix.data(), framesToWrite, NULL);

					// 2. Strong physical friction noise with fast attack on collision spikes
					float oldSmoothedFriction = m_smoothedFriction;
					if (m_frictionLevel > m_smoothedFriction)
					{
						m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * 0.40f; // Smoothed fast attack
					}
					else
					{
						m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * 0.08f; // Natural decay
					}

					if (m_smoothedFriction > 0.0001f || oldSmoothedFriction > 0.0001f)
					{
						ma_noise_read_pcm_frames(&m_noise, temp.data(), framesToWrite, NULL);
						float oldNoiseGain = (std::min)(1.0f, oldSmoothedFriction * 1.10f);
						float newNoiseGain = (std::min)(1.0f, m_smoothedFriction * 1.10f);

						for (ma_uint32 i = 0; i < framesToWrite; ++i)
						{
							// Interpolate gain across chunk to avoid clicking/popping
							float t = static_cast<float>(i) / static_cast<float>(framesToWrite);
							float currentGain = oldNoiseGain + t * (newNoiseGain - oldNoiseGain);
							for (ma_uint32 c = 0; c < CHANNELS; ++c)
							{
								mix[i * CHANNELS + c] += temp[i * CHANNELS + c] * currentGain;
							}
						}
					}

					// 3. Polyphonic Synth Voices with 1-Pole Low-Pass Filter & Stereo Panning
					const float ATTACK_TIME = 0.008f;
					for (auto& voice : m_activeVoices)
					{
						if (voice.finished)
							continue;

						const float phaseInc = 2.0f * static_cast<float>(M_PI) * voice.frequency / SAMPLE_RATE;
						const float wc = 2.0f * static_cast<float>(M_PI) *
										 (std::clamp)(voice.filterCutoff, 20.0f, 20000.0f) / SAMPLE_RATE;
						const float filterAlpha = std::clamp(wc / (wc + 1.0f), 0.001f, 1.0f);

						for (ma_uint32 i = 0; i < framesToWrite; ++i)
						{
							float env = voice.amplitude * expf(-voice.time / voice.decay);
							if (voice.time < ATTACK_TIME)
							{
								env *= (voice.time / ATTACK_TIME);
							}

							float rawSample = 0.0f;
							switch (voice.instrument)
							{
								case InstrumentType::Sawtooth:
									rawSample = 1.0f - (voice.phase / static_cast<float>(M_PI));
									break;
								case InstrumentType::Square:
									rawSample = (voice.phase < static_cast<float>(M_PI)) ? 0.6f : -0.6f;
									break;
								case InstrumentType::Noise:
									rawSample =
										(static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
									break;
								case InstrumentType::Polyphonic:
									// Rich harmonic chime / EP timbre: fundamental + subtle 2nd & 3rd overtone
									rawSample = 0.68f * sinf(voice.phase) + 0.22f * sinf(voice.phase * 2.0f) +
												0.10f * sinf(voice.phase * 3.0f);
									break;
								case InstrumentType::Sine:
								default:
									rawSample = sinf(voice.phase);
									break;
							}

							// Apply 1-pole lowpass filter
							voice.filterState += filterAlpha * (rawSample - voice.filterState);
							float sample = env * voice.filterState;

							voice.phase += phaseInc;
							if (voice.phase >= 2.0f * static_cast<float>(M_PI))
								voice.phase -= 2.0f * static_cast<float>(M_PI);
							voice.time += 1.0f / SAMPLE_RATE;

							mix[i * CHANNELS + 0] += sample * voice.leftGain;
							mix[i * CHANNELS + 1] += sample * voice.rightGain;
						}

						if (voice.time > ATTACK_TIME && voice.amplitude * expf(-voice.time / voice.decay) < 0.001f)
						{
							voice.finished = true;
						}
					}

					// Cleanup finished voices
					m_activeVoices.erase(std::remove_if(m_activeVoices.begin(), m_activeVoices.end(),
														[](const CollisionVoice& v) { return v.finished; }),
										 m_activeVoices.end());

					// 4. Soft clipping
					for (size_t i = 0; i < mix.size(); ++i)
					{
						mix[i] = std::tanh(mix[i]);
					}

					// 5. Visual data snapshot
					float sumSquares = 0.0f;
					for (size_t i = 0; i < mix.size(); i += 4)
					{
						sumSquares += mix[i] * mix[i];
					}
					float rms = std::sqrt(sumSquares / (mix.size() / 4));
					m_visualSnapshot.currentVolume = rms;
					m_visualSnapshot.currentFriction = m_smoothedFriction;
					size_t captureSize = (std::min)(static_cast<size_t>(128), mix.size());
					m_visualSnapshot.waveform.resize(captureSize);
					std::copy(mix.end() - captureSize, mix.end(), m_visualSnapshot.waveform.begin());

					// 6. Submit to SDL stream
					SDL_PutAudioStreamData(m_audioStream, mix.data(),
										   static_cast<int>(framesToWrite * CHANNELS * sizeof(float)));
				}
			}
		}

		// ------------------- Procedural Controls -------------------

		void AudioEngine::setFrictionLevel(float level)
		{
			// Normalize
			constexpr float MAX_FRICTION = 1.0f;
			const float normalizedFriction = (std::min)(level / MAX_FRICTION, 1.0f);

			// Remove min audible and compensate
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
				float finalAmplitude = COMPRESSION_THRESHOLD + compressedOvershoot;
				m_frictionLevel = finalAmplitude;
			}
			else
			{
				m_frictionLevel = initialAmplitude;
			}
		}

		void AudioEngine::playVoice(float freq, float amp, float decaySec, InstrumentType instrument, float leftGain,
									float rightGain, float filterCutoff)
		{
			if (amp <= 0.001f)
				return;

			CollisionVoice newVoice;
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

			if (m_activeVoices.size() >= MAX_ACTIVE_VOICES)
			{
				// Find voice with lowest remaining envelope or oldest
				size_t victimIdx = 0;
				float minCurrentAmp = 1000.0f;
				for (size_t i = 0; i < m_activeVoices.size(); ++i)
				{
					float curAmp =
						m_activeVoices[i].amplitude * expf(-m_activeVoices[i].time / m_activeVoices[i].decay);
					if (curAmp < minCurrentAmp)
					{
						minCurrentAmp = curAmp;
						victimIdx = i;
					}
				}
				if (newVoice.amplitude > minCurrentAmp * 0.5f)
				{
					m_activeVoices[victimIdx] = newVoice;
				}
				return;
			}

			m_activeVoices.push_back(newVoice);
		}

		void AudioEngine::playSineSound(float freq, float amp, float decaySec)
		{
			playVoice(freq, amp, decaySec, InstrumentType::Sine, 0.7071f, 0.7071f, 20000.0f);
		}

		AudioData AudioEngine::getAudioData()
		{
			return m_visualSnapshot;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
