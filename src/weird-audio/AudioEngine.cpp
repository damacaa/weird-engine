#include "weird-audio/AudioEngine.h"
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

#define CHANNELS 2
#define SAMPLE_RATE 44100

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		AudioEngine::AudioEngine() {}

		AudioEngine::~AudioEngine() {}

		bool AudioEngine::init(const WeirdAudio::AudioSettings& settings)
		{
			m_settings = settings;

			m_physicsEngine.init(SAMPLE_RATE, CHANNELS);
			m_physicsEngine.setSpatialAudioEnabled(settings.enableSpatialAudio);
			m_physicsEngine.setVolume(settings.physicsVolume);

			m_musicEngine.init(SAMPLE_RATE, CHANNELS);
			m_musicEngine.setVolume(settings.musicVolume);

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

			return true;
		}

		void AudioEngine::close()
		{
			m_audioStream = nullptr;
			if (m_hasSound)
			{
				ma_sound_uninit(&m_sound);
				m_hasSound = false;
			}
			ma_engine_uninit(&m_engine);
		}

		void AudioEngine::loadSound(const char* filePath)
		{
			if (m_hasSound)
			{
				ma_sound_uninit(&m_sound);
				m_hasSound = false;
			}

			ma_result result = ma_sound_init_from_file(&m_engine, filePath, 0, NULL, NULL, &m_sound);
			if (result != MA_SUCCESS)
			{
				WeirdEngine::Logger::error("Failed to initialize sound from file: " + std::string(filePath));
				return;
			}

			ma_sound_set_looping(&m_sound, MA_TRUE);
			ma_sound_set_volume(&m_sound, 0.5f);
			ma_sound_start(&m_sound);
			m_hasSound = true;
		}

		ma_uint32 AudioEngine::getSampleRate() const
		{
			return SAMPLE_RATE;
		}

		ma_uint8 AudioEngine::getChannels() const
		{
			return CHANNELS;
		}

		AudioData AudioEngine::getAudioData()
		{
			return m_visualSnapshot;
		}

		void AudioEngine::listen(Scene& scene)
		{
			if (m_settings.mute || !m_audioStream)
				return;

			// 1. Camera & Listener orientation
			auto& camera = scene.getCamera();
			vec3 listenerPos = camera.position;
			vec3 listenerForward = glm::length2(camera.orientation) > 0.001f ? glm::normalize(camera.orientation)
																			 : vec3(0.0f, 0.0f, -1.0f);
			vec3 listenerUp = glm::length2(camera.up) > 0.001f ? glm::normalize(camera.up) : vec3(0.0f, 1.0f, 0.0f);

			// 2. Physics continuous friction
			float frictionValue = scene.getFrictionSound();
			m_physicsEngine.setFrictionLevel(frictionValue);

			// 3. Drain and process physics requests from Scene
			auto& audioQueue = scene.getAudioQueue();
			SimpleAudioRequest req;
			while (audioQueue.pop(req))
			{
				// For non-noise sounds without explicit frequency, quantize to current active song scale
				if (req.instrument != 3 && req.frequency <= 0.0f)
				{
					req.frequency = m_musicEngine.quantizeToSongScale(req.frequency);
				}

				m_physicsEngine.playSound(req, listenerPos, listenerForward, listenerUp);
			}

			// 4. Update procedural music timing & dynamic feedback
			m_musicEngine.update(scene.getLastDelta(), scene.getTime());

			// 6. Generate and stream PCM audio to SDL
			constexpr int TARGET_BUFFER_BYTES = (SAMPLE_RATE * CHANNELS * sizeof(float) * 8) / 100;
			int queuedBytes = SDL_GetAudioStreamQueued(m_audioStream);

			if (queuedBytes < TARGET_BUFFER_BYTES)
			{
				int bytesToGenerate = TARGET_BUFFER_BYTES - queuedBytes;
				ma_uint32 framesToWrite = static_cast<ma_uint32>(bytesToGenerate / (CHANNELS * sizeof(float)));
				if (framesToWrite > 4096)
				{
					framesToWrite = 4096;
				}

				if (framesToWrite > 0)
				{
					std::vector<float> mix(framesToWrite * CHANNELS, 0.0f);

					// Layer 1: Physics realistic sounds & spatial audio
					if (m_settings.enablePhysicsAudio)
					{
						m_physicsEngine.render(mix.data(), framesToWrite, CHANNELS);
					}

					// Layer 2: SDF Procedural Music
					if (m_settings.enableMusic)
					{
						m_musicEngine.render(mix.data(), framesToWrite, CHANNELS);
					}

					// Layer 3: Optional background music track from file
					if (m_hasSound)
					{
						std::vector<float> bgTrack(framesToWrite * CHANNELS, 0.0f);
						ma_engine_read_pcm_frames(&m_engine, bgTrack.data(), framesToWrite, NULL);
						for (size_t i = 0; i < mix.size(); ++i)
						{
							mix[i] += bgTrack[i];
						}
					}

					// Master bus processing: Volume & Soft-Clipping (tanh)
					float masterVol = m_settings.masterVolume;
					for (size_t i = 0; i < mix.size(); ++i)
					{
						mix[i] = std::tanh(mix[i] * masterVol);
					}

					// Visual snapshot (RMS volume + waveform capture)
					float sumSquares = 0.0f;
					for (size_t i = 0; i < mix.size(); i += 4)
					{
						sumSquares += mix[i] * mix[i];
					}
					float rms = std::sqrt(sumSquares / (mix.size() / 4));
					m_visualSnapshot.currentVolume = rms;
					m_visualSnapshot.currentFriction = m_physicsEngine.getFrictionLevel();

					// Extract mono waveform (continuous tail of the mix buffer)
					constexpr size_t WAVEFORM_SAMPLES = 256;
					m_visualSnapshot.waveform.resize(WAVEFORM_SAMPLES, 0.0f);

					if (framesToWrite >= WAVEFORM_SAMPLES)
					{
						size_t startFrame = framesToWrite - WAVEFORM_SAMPLES;
						for (size_t i = 0; i < WAVEFORM_SAMPLES; ++i)
						{
							size_t frame = startFrame + i;
							m_visualSnapshot.waveform[i] = (mix[frame * CHANNELS] + mix[frame * CHANNELS + 1]) * 0.5f;
						}
					}
					else if (framesToWrite > 0)
					{
						for (size_t i = 0; i < WAVEFORM_SAMPLES; ++i)
						{
							size_t frame = (i * framesToWrite) / WAVEFORM_SAMPLES;
							m_visualSnapshot.waveform[i] = (mix[frame * CHANNELS] + mix[frame * CHANNELS + 1]) * 0.5f;
						}
					}

					// Submit to SDL stream
					SDL_PutAudioStreamData(m_audioStream, mix.data(),
										   static_cast<int>(framesToWrite * CHANNELS * sizeof(float)));
				}
			}
		}

	} // namespace WeirdAudio
} // namespace WeirdEngine
