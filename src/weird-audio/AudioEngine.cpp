#include "weird-audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <vector>

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

			return true;
		}

		uint32_t AudioEngine::getSampleRate() const
		{
			return SAMPLE_RATE;
		}

		uint8_t AudioEngine::getChannels() const
		{
			return CHANNELS;
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

			// 2. Physics continuous friction (one voice per selected source)
			if (scene.isFrictionSoundOverridden())
			{
				m_physicsEngine.setFrictionLevel(scene.getFrictionSound(), listenerPos, listenerPos, listenerForward,
												 listenerUp);
			}
			else
			{
				m_physicsEngine.setFrictionSources(scene.getFrictionSources(), listenerPos, listenerForward,
												   listenerUp);
			}

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
			constexpr int TARGET_BUFFER_BYTES = (SAMPLE_RATE * CHANNELS * sizeof(float) * 14) / 100;
			int queuedBytes = SDL_GetAudioStreamQueued(m_audioStream);

			if (queuedBytes < TARGET_BUFFER_BYTES)
			{
				int bytesToGenerate = TARGET_BUFFER_BYTES - queuedBytes;
				uint32_t framesToWrite = static_cast<uint32_t>(bytesToGenerate / (CHANNELS * sizeof(float)));
				if (framesToWrite > 8192)
				{
					framesToWrite = 8192;
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

					// Master bus processing: DC Blocker (1-pole highpass at ~15 Hz, R = 0.995)
					// Eliminates DC offset so waveforms center symmetrically at 0.0
					constexpr float DC_BLOCK_R = 0.995f;
					for (size_t frame = 0; frame < framesToWrite; ++frame)
					{
						for (size_t ch = 0; ch < CHANNELS; ++ch)
						{
							size_t idx = frame * CHANNELS + ch;
							float in = mix[idx];
							float out = in - m_dcBlockerX[ch] + DC_BLOCK_R * m_dcBlockerY[ch];
							m_dcBlockerX[ch] = in;
							// Denormal protection
							if (std::abs(out) < 1e-15f)
								out = 0.0f;
							m_dcBlockerY[ch] = out;
							mix[idx] = out;
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
