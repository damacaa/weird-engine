#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <SDL3/SDL.h>
#include <vector>

#include "weird-audio/AudioSettings.h"
#include "weird-audio/PhysicsAudioEngine.h"
#include "weird-audio/SdfMusicEngine.h"
#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		struct AudioData
		{
			float currentVolume = 0.0f;	  // For pulsing size
			float currentFriction = 0.0f; // For static/jitter
			std::vector<float> waveform;  // For oscilloscope effects (snapshot of last 256 samples)

			AudioData()
				: waveform(256, 0.0f)
			{
			}
		};

		class AudioEngine
		{
		public:
			static AudioEngine& getInstance()
			{
				static AudioEngine instance;
				return instance;
			}

			AudioEngine(const AudioEngine&) = delete;
			AudioEngine& operator=(const AudioEngine&) = delete;

			~AudioEngine();

			bool init(const WeirdAudio::AudioSettings& settings);

			void setAudioStream(SDL_AudioStream* stream)
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				m_audioStream = stream;
			}

			uint32_t getSampleRate() const;
			uint8_t getChannels() const;

			// SDL3 Audio stream callback invoked on the dedicated audio thread
			static void SDLCALL audioStreamCallback(void* userdata, SDL_AudioStream* stream, int additional_amount,
													int total_amount);

			// Generates PCM audio data directly on the audio thread
			void renderAudio(SDL_AudioStream* stream, int additional_amount);

			// Per-frame scene audio synchronization from main thread
			void listen(Scene& scene);

			// Volume & Mute controls
			void mute()
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				m_settings.mute = true;
			}

			void unmute()
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				m_settings.mute = false;
			}

			bool isMuted() const
			{
				return m_settings.mute;
			}

			void setMasterVolume(float vol)
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				m_settings.masterVolume = vol;
			}

			float getMasterVolume() const
			{
				return m_settings.masterVolume;
			}

			// Subsystem access
			PhysicsAudioEngine& getPhysicsEngine()
			{
				return m_physicsEngine;
			}

			const PhysicsAudioEngine& getPhysicsEngine() const
			{
				return m_physicsEngine;
			}

			SdfMusicEngine& getMusicEngine()
			{
				return m_musicEngine;
			}

			const SdfMusicEngine& getMusicEngine() const
			{
				return m_musicEngine;
			}

			// Spatial Audio Setting
			void setSpatialAudioEnabled(bool enabled)
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				m_settings.enableSpatialAudio = enabled;
				m_physicsEngine.setSpatialAudioEnabled(enabled);
			}

			bool isSpatialAudioEnabled() const
			{
				return m_physicsEngine.isSpatialAudioEnabled();
			}

			// Visualizer data
			AudioData getAudioData() const
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				return m_visualSnapshot;
			}

			float getAudioVolume() const
			{
				std::lock_guard<std::mutex> lock(m_audioMutex);
				return m_visualSnapshot.currentVolume;
			}

		private:
			AudioEngine();

			WeirdAudio::AudioSettings m_settings;
			PhysicsAudioEngine m_physicsEngine;
			SdfMusicEngine m_musicEngine;

			mutable std::mutex m_audioMutex;
			SDL_AudioStream* m_audioStream = nullptr;
			AudioData m_visualSnapshot;
			std::vector<float> m_mixBuffer;
			double m_audioTime = 0.0;

			// Master bus DC-blocking filter state (per channel)
			float m_dcBlockerX[2] = {0.0f, 0.0f};
			float m_dcBlockerY[2] = {0.0f, 0.0f};
		};

	} // namespace WeirdAudio
} // namespace WeirdEngine
