#include "weird-renderer/audio/AudioEngine.h"
#include "weird-engine/Logger.h"
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

		constexpr int MAX_ACTIVE_VOICES = 16;

		AudioEngine::AudioEngine()
			: m_mute(false)
		{
		}

		AudioEngine::~AudioEngine() {}

		bool AudioEngine::init(const AudioSettings& settings)
		{
			m_mute = settings.mute;
			m_enableAmbient = settings.enableAmbient;
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
			// This removes notes that create high tension (like F and B)
			std::vector<int> allowedIntervals = {0, 2, 4, 7, 9};

			// Find the note relative to C (MIDI % 12)
			// We strictly want notes where (note % 12) matches our allowed intervals
			// If the current note isn't allowed, find the closest one that is.

			int closestNote = roundedNote;
			float minDistance = 100.0f;

			// Search neighboring notes to find the closest allowed note
			for (int offset = -2; offset <= 2; ++offset)
			{
				int candidate = roundedNote + offset;
				int interval = candidate % 12; // Modulo 12 gets the note name (C, C#, etc.)

				// Handle negative modulo results if freq is very low
				if (interval < 0)
					interval += 12;

				// Check if this interval is in our allowed list
				for (int allowed : allowedIntervals)
				{
					if (interval == allowed)
					{
						// If this allowed note is closer to original, pick it
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
			setFrictionLevel(frictionValue);

			auto& audioQueue = scene.getAudioQueue();

			constexpr float BASE_BEAT = 0.1f;

			constexpr static float MIN_COLLISION_INTERVAL = BASE_BEAT; // Min time between collision sounds
			constexpr static float SILENCE_TIME_THRESHOLD =
				BASE_BEAT *
				64; // When there are no collisions, start adding collision sounds to generate procedural music
			constexpr static float AMBIENT_NOTE_INTERVAL = BASE_BEAT * 8; // Time between procedural music sounds
			constexpr float MAX_AMBIENT_VOLUME = 0.2f;
			constexpr float AMBIENT_FADE_IN_SPEED = 0.1f * MAX_AMBIENT_VOLUME;
			constexpr float LIKELIHOOD_OF_DOUBLE_BEAT = 0.3f;
			constexpr float LIKELIHOOD_OF_SKIP_BEAT = 0.2f;

			static int mergedCollisionCount = 0;
			static SimpleAudioRequest accumulatedSoundData{0.0f, 0.0f, true, vec3(0.0f)};
			static float nextAllowedPlayTime = MIN_COLLISION_INTERVAL;
			static float nextAllowedAmbientPlayTime = AMBIENT_NOTE_INTERVAL;
			static float ambientStartTime = SILENCE_TIME_THRESHOLD;

			static bool isPlayingAmbience = false;
			static float currentAmbientVolume = 0.0f;
			static float previousFrameTime = 0.0f;

			static std::random_device randDevice;
			static std::mt19937 generator(randDevice());

			// Time
			float time = scene.getTime();
			float deltaTime = time - previousFrameTime;
			if (deltaTime < 0.0f)
			{
				// New scene
				deltaTime = 0.0f;

				nextAllowedPlayTime = time + MIN_COLLISION_INTERVAL;
				nextAllowedAmbientPlayTime = time + AMBIENT_NOTE_INTERVAL;
				ambientStartTime = time + SILENCE_TIME_THRESHOLD;
			}
			previousFrameTime = time;

			if (m_enableAmbient && audioQueue.empty())
			{
				// Fade fill music in
				currentAmbientVolume =
					(std::min)(currentAmbientVolume + (AMBIENT_FADE_IN_SPEED * deltaTime), MAX_AMBIENT_VOLUME);

				// Procedural ambient
				if ((!isPlayingAmbience && time > ambientStartTime) ||
					(isPlayingAmbience && time > nextAllowedAmbientPlayTime))
				{
					if (!isPlayingAmbience)
					{
						isPlayingAmbience = true;
						currentAmbientVolume = 0.05f;
					}

					const int minFrequency = 200;
					const int maxFrequency = 350;

					std::uniform_int_distribution<int> distribution(minFrequency, maxFrequency);

					int randomFrequency = distribution(generator);
					SimpleAudioRequest aux{currentAmbientVolume, static_cast<float>(randomFrequency), false,
										   vec3(0.0f)};
					audioQueue.push(aux);
				}
			}
			else
			{
				// Stop ambient
				isPlayingAmbience = false;
				nextAllowedAmbientPlayTime = time + AMBIENT_NOTE_INTERVAL;

				SimpleAudioRequest aux{0, 0, true, vec3(0.0f)};
				while (audioQueue.pop(aux))
				{
					mergedCollisionCount++;
					accumulatedSoundData.volume += aux.volume;
					accumulatedSoundData.frequency = (std::max)(aux.frequency, accumulatedSoundData.frequency);
					accumulatedSoundData.position += accumulatedSoundData.position;
					accumulatedSoundData.spatial = accumulatedSoundData.spatial || aux.spatial;
					accumulatedSoundData.beats = (std::max)(aux.beats, accumulatedSoundData.beats);
				}

				if (time >= nextAllowedPlayTime && m_activeVoices.size() < MAX_ACTIVE_VOICES)
				{
					float invBufferedAmount = 1.0f / static_cast<float>(mergedCollisionCount + 1);
					accumulatedSoundData.volume = (std::min)(accumulatedSoundData.volume, 1.0f);
					accumulatedSoundData.position *= invBufferedAmount;
					audioQueue.push(accumulatedSoundData);

					mergedCollisionCount = 0;
					accumulatedSoundData = SimpleAudioRequest{0.0f, 0.0f, false, vec3(0.0f), 1};
				}
			}

			auto cameraPosition = scene.getCamera().position;

			// Process queue
			SimpleAudioRequest request{};
			while (audioQueue.pop(request))
			{
				float frequency = getPleasantFrequency(request.frequency);
				float amplitude = request.volume;
				if (request.spatial)
				{
					// 1. Calculate Distances
					float distSq = glm::distance2(request.position, cameraPosition);
					float dist = std::sqrt(distSq);

					// --- PHYSICS CONSTANTS ---
					const float FALLOFF_GEOMETRIC = 0.001f;
					const float FALLOFF_AIR_ABSORPTION = 0.00005f;

					// 2. Geometric Spreading (Inverse Distance Model)
					float geometricFactor = 1.0f / (1.0f + (FALLOFF_GEOMETRIC * distSq));

					// 3. Atmospheric Absorption (Exponential Decay)
					float absorptionFactor = std::exp(-FALLOFF_AIR_ABSORPTION * frequency * dist);

					// Combine factors
					amplitude = request.volume * geometricFactor * absorptionFactor;
				}

				// Optimization: Don't play sounds that are effectively silent
				if (amplitude > 0.01f)
				{
					std::bernoulli_distribution durationDistribution(LIKELIHOOD_OF_DOUBLE_BEAT);
					float duration = durationDistribution(generator) ? 2.0f : 1.0f;

					float decay = MIN_COLLISION_INTERVAL * duration * static_cast<float>(request.beats);
					nextAllowedPlayTime = time + decay;

					bool skipBeat = false;
					if (!isPlayingAmbience)
					{
						// Reset ambient timer
						ambientStartTime = time + SILENCE_TIME_THRESHOLD;
					}
					else
					{
						decay = AMBIENT_NOTE_INTERVAL * duration;
						nextAllowedAmbientPlayTime = time + decay;
						decay *= 0.5f;

						std::bernoulli_distribution skipBeatDistribution(LIKELIHOOD_OF_SKIP_BEAT);
						skipBeat = skipBeatDistribution(generator);
					}

					if (!skipBeat)
						playSineSound(frequency, amplitude, decay);

					break;
				}
			}

			// --- Generate and push audio frames to SDL stream ---
			// Maintain ~80ms buffer (approx 3528 samples at 44.1kHz stereo)
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

					// 2. Friction noise
					m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * 0.10f;
					if (m_smoothedFriction > 0.0001f)
					{
						ma_noise_read_pcm_frames(&m_noise, temp.data(), framesToWrite, NULL);
						for (ma_uint32 i = 0; i < framesToWrite * CHANNELS; ++i)
						{
							mix[i] += temp[i] * m_smoothedFriction * 0.45f;
						}
					}

					// 3. Collision Tones (Polyphonic)
					const float ATTACK_TIME = 0.01f;
					for (auto& voice : m_activeVoices)
					{
						if (voice.finished)
							continue;

						const float phaseInc = 2.0f * M_PI * voice.frequency / SAMPLE_RATE;

						for (ma_uint32 i = 0; i < framesToWrite; ++i)
						{
							float env = voice.amplitude * expf(-voice.time / voice.decay);
							if (voice.time < ATTACK_TIME)
							{
								env *= (voice.time / ATTACK_TIME);
							}

							float sample = env * sinf(voice.phase);
							voice.phase += phaseInc;
							if (voice.phase >= 2.0f * M_PI)
								voice.phase -= 2.0f * M_PI;
							voice.time += 1.0f / SAMPLE_RATE;

							for (int ch = 0; ch < CHANNELS; ++ch)
							{
								mix[i * CHANNELS + ch] += sample;
							}
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

			// Use a square root curve to boost the volume of low values significantly
			// 0.5f = Strong boost (square root)
			// 0.75f = Medium boost
			// 1.0f = No boost (linear)
			constexpr float LOW_END_BOOST_EXPONENT = 0.5f;
			constexpr float MAX_AMPLITUDE = 0.75f;
			const float initialAmplitude = MAX_AMPLITUDE * pow(adjustedAmplitude, LOW_END_BOOST_EXPONENT);

			// Compression will tame the louder signal once it crosses the threshold, preserving top-end
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

		void AudioEngine::playSineSound(float freq, float amp, float decaySec)
		{
			if (m_activeVoices.size() >= MAX_ACTIVE_VOICES)
				return;

			CollisionVoice newVoice;
			newVoice.frequency = freq;
			newVoice.amplitude = amp;
			newVoice.decay = decaySec;
			newVoice.time = 0.0f;
			newVoice.phase = 0.0f;
			newVoice.finished = false;

			m_activeVoices.push_back(newVoice);
		}

		AudioData AudioEngine::getAudioData()
		{
			return m_visualSnapshot;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
