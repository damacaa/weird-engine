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
			std::lock_guard<std::mutex> lock(m_audioMutex);
			m_settings = settings;

			m_physicsEngine.init(SAMPLE_RATE, CHANNELS);
			m_physicsEngine.setSpatialAudioEnabled(settings.enableSpatialAudio);
			m_physicsEngine.setVolume(settings.physicsVolume);

			m_musicEngine.init(SAMPLE_RATE, CHANNELS);
			m_musicEngine.setVolume(settings.musicVolume);

			m_mixBuffer.resize(8192 * CHANNELS, 0.0f);
			m_visualSnapshot.waveform.assign(256, 0.0f);
			m_audioTime = 0.0;

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
			{
				auto& audioQueue = scene.getAudioQueue();
				SimpleAudioRequest req;
				while (audioQueue.pop(req))
				{
				}
				return;
			}

			// 1. Camera & Listener orientation
			auto& camera = scene.getCamera();
			vec3 listenerPos = camera.position;
			vec3 listenerForward = glm::length2(camera.orientation) > 0.001f ? glm::normalize(camera.orientation)
																			 : vec3(0.0f, 0.0f, -1.0f);
			vec3 listenerUp = glm::length2(camera.up) > 0.001f ? glm::normalize(camera.up) : vec3(0.0f, 1.0f, 0.0f);

			std::lock_guard<std::mutex> lock(m_audioMutex);

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
		}

		void SDLCALL AudioEngine::audioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
													  int /*total_amount*/)
		{
			auto* self = static_cast<AudioEngine*>(userdata);
			if (!self)
				return;

			self->renderAudio(stream, additional_amount);
		}

		void AudioEngine::renderAudio(SDL_AudioStream* stream, int additional_amount)
		{
			if (!stream)
				return;

			constexpr int bytesPerFrame = CHANNELS * sizeof(float);
			int bytesNeeded = additional_amount;
			if (bytesNeeded <= 0)
			{
				return;
			}

			constexpr int MAX_CHUNK_FRAMES = 4096;
			int maxBytes = MAX_CHUNK_FRAMES * bytesPerFrame;
			if (bytesNeeded > maxBytes)
			{
				bytesNeeded = maxBytes;
			}

			uint32_t framesToWrite = static_cast<uint32_t>((bytesNeeded + bytesPerFrame - 1) / bytesPerFrame);
			if (framesToWrite == 0)
				return;

			std::lock_guard<std::mutex> lock(m_audioMutex);

			// Advance procedural music sample-accurately based on rendered audio frames
			double chunkDt = static_cast<double>(framesToWrite) / static_cast<double>(SAMPLE_RATE);
			m_audioTime += chunkDt;
			m_musicEngine.update(chunkDt, m_audioTime);

			size_t totalSamples = framesToWrite * CHANNELS;
			if (m_mixBuffer.size() < totalSamples)
			{
				m_mixBuffer.resize(totalSamples, 0.0f);
			}
			std::fill(m_mixBuffer.begin(), m_mixBuffer.begin() + totalSamples, 0.0f);

			if (!m_settings.mute)
			{
				// Layer 1: Physics realistic sounds & spatial audio
				if (m_settings.enablePhysicsAudio)
				{
					m_physicsEngine.render(m_mixBuffer.data(), framesToWrite, CHANNELS);
				}

				// Layer 2: SDF Procedural Music
				if (m_settings.enableMusic)
				{
					m_musicEngine.render(m_mixBuffer.data(), framesToWrite, CHANNELS);
				}

				// Master bus processing: DC Blocker (1-pole highpass at ~15 Hz, R = 0.995)
				constexpr float DC_BLOCK_R = 0.995f;
				for (size_t frame = 0; frame < framesToWrite; ++frame)
				{
					for (size_t ch = 0; ch < CHANNELS; ++ch)
					{
						size_t idx = frame * CHANNELS + ch;
						float in = m_mixBuffer[idx];
						float out = in - m_dcBlockerX[ch] + DC_BLOCK_R * m_dcBlockerY[ch];
						m_dcBlockerX[ch] = in;
						if (std::abs(out) < 1e-15f)
							out = 0.0f;
						m_dcBlockerY[ch] = out;
						m_mixBuffer[idx] = out;
					}
				}

				// Master bus processing: Volume & Soft-Clipping (tanh)
				float masterVol = m_settings.masterVolume;
				for (size_t i = 0; i < totalSamples; ++i)
				{
					m_mixBuffer[i] = std::tanh(m_mixBuffer[i] * masterVol);
				}
			}

			// Visual snapshot (RMS volume + waveform capture)
			float sumSquares = 0.0f;
			for (size_t i = 0; i < totalSamples; i += 4)
			{
				sumSquares += m_mixBuffer[i] * m_mixBuffer[i];
			}
			float rms = std::sqrt(sumSquares / (totalSamples / 4 + 1));
			m_visualSnapshot.currentVolume = rms;
			m_visualSnapshot.currentFriction = m_physicsEngine.getFrictionLevel();

			// Extract mono waveform
			constexpr size_t WAVEFORM_SAMPLES = 256;
			if (m_visualSnapshot.waveform.size() != WAVEFORM_SAMPLES)
			{
				m_visualSnapshot.waveform.resize(WAVEFORM_SAMPLES, 0.0f);
			}

			if (framesToWrite >= WAVEFORM_SAMPLES)
			{
				size_t startFrame = framesToWrite - WAVEFORM_SAMPLES;
				for (size_t i = 0; i < WAVEFORM_SAMPLES; ++i)
				{
					size_t frame = startFrame + i;
					m_visualSnapshot.waveform[i] =
						(m_mixBuffer[frame * CHANNELS] + m_mixBuffer[frame * CHANNELS + 1]) * 0.5f;
				}
			}
			else if (framesToWrite > 0)
			{
				for (size_t i = 0; i < WAVEFORM_SAMPLES; ++i)
				{
					size_t frame = (i * framesToWrite) / WAVEFORM_SAMPLES;
					m_visualSnapshot.waveform[i] =
						(m_mixBuffer[frame * CHANNELS] + m_mixBuffer[frame * CHANNELS + 1]) * 0.5f;
				}
			}

			// Submit to SDL stream
			SDL_PutAudioStreamData(stream, m_mixBuffer.data(), static_cast<int>(totalSamples * sizeof(float)));
		}

	} // namespace WeirdAudio
} // namespace WeirdEngine
