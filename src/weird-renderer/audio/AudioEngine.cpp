#include "weird-renderer/audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
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
			ma_result result;
			ma_engine_config engineConfig = ma_engine_config_init();
			engineConfig.noDevice = MA_TRUE;
			engineConfig.channels = CHANNELS;
			engineConfig.sampleRate = SAMPLE_RATE;

			result = ma_engine_init(&engineConfig, &m_engine);
			if (result != MA_SUCCESS)
			{
				std::cerr << "Failed to initialize audio engine" << std::endl;
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
				std::cerr << "Failed to initialize noise generator" << std::endl;
				return false;
			}

			return true;
		}

		void AudioEngine::close()
		{
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
			int minDistance = 100;

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
						if (std::abs(candidate - continuousNote) < minDistance)
						{
							minDistance = std::abs(candidate - continuousNote);
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
			if (m_mute)
				return;

			if (Input::GetKeyDown(Input::C))
				playSineSound(getPleasantFrequency(200.0f), 1.0f, 0.1f);

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
				// playSineSound(getPleasantFrequency(80), MAX_FILL_VOLUME, 1.5f);

				nextAllowedPlayTime = time + MIN_COLLISION_INTERVAL;
				nextAllowedAmbientPlayTime = time + AMBIENT_NOTE_INTERVAL;
				ambientStartTime = time + SILENCE_TIME_THRESHOLD;
			}
			previousFrameTime = time;

			if (audioQueue.empty())
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
					// std::cout << "Playing: " << randomFrequency << "Hz" << currentAmbientVolume << std::endl;
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
					// buffered_request.volume *= invBufferedAmount;
					accumulatedSoundData.volume = (std::min)(accumulatedSoundData.volume, 1.0f);
					// buffered_request.frequency *= invBufferedAmount;
					accumulatedSoundData.position *= invBufferedAmount;
					audioQueue.push(accumulatedSoundData);
					// std::cout << "Playing: " << static_cast<int>(buffered_request.volume * 100) << "% -> " <<
					// buffered_request.frequency << "Hz" << std::endl;

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
					// We need squared distance for spreading, and linear distance for absorption
					float distSq = glm::distance2(request.position, cameraPosition);
					float dist = std::sqrt(distSq);

					// --- PHYSICS CONSTANTS (Tune these to your game scale) ---
					// Controls how fast sound dies in general (Inverse Square Law)
					const float FALLOFF_GEOMETRIC = 0.001f;

					// Controls how much air "eats" sound.
					// Real world is approx 0.000002, but in games use higher values (e.g. 0.00005)
					// to exaggerate the effect so players notice it.
					const float FALLOFF_AIR_ABSORPTION = 0.00005f;

					// 2. Geometric Spreading (Inverse Distance Model)
					// Energy dissipates over area. Affects all frequencies equally.
					// Using (1.0 + ...) prevents division by zero at close range.
					float geometricFactor = 1.0f / (1.0f + (FALLOFF_GEOMETRIC * distSq));

					// 3. Atmospheric Absorption (Exponential Decay)
					// Formula: e^(-coefficient * frequency * distance)
					// Higher Freq = Faster Decay. Further Dist = More Decay.
					float absorptionFactor = std::exp(-FALLOFF_AIR_ABSORPTION * frequency * dist);

					// Combine factors
					amplitude = request.volume * geometricFactor * absorptionFactor;
				}

				// Optimization: Don't play sounds that are effectively silent
				if (amplitude > 0.01f)
				{
					// std::bernoulli_distribution returns true based on the given probability
					std::bernoulli_distribution durationDistribution(LIKELIHOOD_OF_DOUBLE_BEAT);

					// If true return 2, if false return 1
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
			// Lock the mutex so the audio thread doesn't read while we write
			std::lock_guard<std::mutex> lock(m_voiceMutex);

			// Optional: Limit max polyphony to prevent CPU overload
			if (m_activeVoices.size() > MAX_ACTIVE_VOICES)
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
			std::lock_guard<std::mutex> lock(m_visualMutex);
			return m_visualSnapshot;
		}

		// ------------------- Updated Callback -------------------

		void AudioEngine::data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount,
										int total_amount)
		{
			AudioEngine* audio = static_cast<AudioEngine*>(pUserData);
			if (!audio)
				return;

			ma_uint32 bytesToWrite = (ma_uint32)total_amount;
			ma_uint32 framesToWrite = bytesToWrite / ma_get_bytes_per_frame(ma_format_f32, CHANNELS);

			// Initialize mix buffer with silence
			std::vector<float> mix(framesToWrite * CHANNELS, 0.0f);
			std::vector<float> temp(framesToWrite * CHANNELS);

			// --- 1. Mix background music (MiniAudio) ---
			ma_engine_read_pcm_frames(&audio->m_engine, mix.data(), framesToWrite, NULL);

			// --- 2. Friction noise ---
			audio->m_smoothedFriction += (audio->m_frictionLevel - audio->m_smoothedFriction) * 0.05f;
			if (audio->m_smoothedFriction > 0.0001f)
			{
				ma_noise_read_pcm_frames(&audio->m_noise, temp.data(), framesToWrite, NULL);
				for (ma_uint32 i = 0; i < framesToWrite * CHANNELS; ++i)
				{
					mix[i] += temp[i] * audio->m_smoothedFriction * 0.4f;
				}
			}

			// --- 3. Collision Tones (Polyphonic) ---

			// Lock mutex to safely access the vector
			// Lock mutex to safely access the vector
			{
				std::lock_guard<std::mutex> lock(audio->m_voiceMutex);

				// Define a short attack time (e.g., 10ms) to prevent popping
				const float ATTACK_TIME = 0.01f;

				for (auto& voice : audio->m_activeVoices)
				{
					if (voice.finished)
						continue;

					const float phaseInc = 2.0f * M_PI * voice.frequency / SAMPLE_RATE;

					for (ma_uint32 i = 0; i < framesToWrite; ++i)
					{
						// 1. Calculate the standard decay envelope
						float env = voice.amplitude * expf(-voice.time / voice.decay);

						// 2. APPLY ATTACK RAMP (The Fix)
						// If time is less than ATTACK_TIME, scale from 0.0 to 1.0
						if (voice.time < ATTACK_TIME)
						{
							env *= (voice.time / ATTACK_TIME);
						}

						float sample = env * sinf(voice.phase);

						voice.phase += phaseInc;
						if (voice.phase >= 2.0f * M_PI)
							voice.phase -= 2.0f * M_PI;
						voice.time += 1.0f / SAMPLE_RATE;

						// Add to both channels
						for (int ch = 0; ch < CHANNELS; ++ch)
						{
							mix[i * CHANNELS + ch] += sample;
						}
					}

					// Check if voice is effectively silent
					if (voice.time > ATTACK_TIME && voice.amplitude * expf(-voice.time / voice.decay) < 0.001f)
					{
						voice.finished = true;
					}
				}

				// Cleanup finished voices to keep vector small
				// Using remove_if idiom
				audio->m_activeVoices.erase(std::remove_if(audio->m_activeVoices.begin(), audio->m_activeVoices.end(),
														   [](const CollisionVoice& v) { return v.finished; }),
											audio->m_activeVoices.end());
			}

			// --- 4. HARD LIMITER / CLIPPING PROTECTION ---
			// Because we are adding multiple sounds, volume might exceed 1.0.
			// We apply a "soft clip" using tanh() or simple clamping to prevent distortion.
			for (size_t i = 0; i < mix.size(); ++i)
			{
				// Soft clipping (smoothly limits to -1.0 to 1.0 range)
				mix[i] = std::tanh(mix[i]);
			}

			// --- NEW: CALCULATE VISUAL DATA ---
			// Do this AFTER mixing but BEFORE SDL_PutAudioStreamData
			if (audio)
			{
				float sumSquares = 0.0f;

				// 1. Calculate RMS (Volume)
				// We iterate with a stride to save CPU (checking every 4th sample is enough for visuals)
				for (size_t i = 0; i < mix.size(); i += 4)
				{
					sumSquares += mix[i] * mix[i];
				}
				float rms = std::sqrt(sumSquares / (mix.size() / 4));

				// 2. Update the shared snapshot
				{
					// Try_lock allows us to skip an update if the render thread is currently reading.
					// This prevents the audio thread from stuttering.
					if (audio->m_visualMutex.try_lock())
					{
						audio->m_visualSnapshot.currentVolume = rms;
						audio->m_visualSnapshot.currentFriction = audio->m_smoothedFriction;

						// Grab a small chunk of the wave for visuals (e.g., last 128 samples)
						size_t captureSize = (std::min)((size_t)128, mix.size());
						audio->m_visualSnapshot.waveform.resize(captureSize);
						std::copy(mix.end() - captureSize, mix.end(), audio->m_visualSnapshot.waveform.begin());

						audio->m_visualMutex.unlock();
					}
				}
			}

			// --- 5. Submit to SDL ---
			SDL_PutAudioStreamData(stream, mix.data(), framesToWrite * ma_get_bytes_per_frame(ma_format_f32, CHANNELS));
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
