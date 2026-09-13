#pragma once

#include <cstdint>
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
				m_audioStream = stream;
			}

			uint32_t getSampleRate() const;
			uint8_t getChannels() const;

			// Per-frame scene audio update and PCM generation
			void listen(Scene& scene);

			// Volume & Mute controls
			void mute()
			{
				m_settings.mute = true;
			}

			void unmute()
			{
				m_settings.mute = false;
			}

			bool isMuted() const
			{
				return m_settings.mute;
			}

			void setMasterVolume(float vol)
			{
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
				m_settings.enableSpatialAudio = enabled;
				m_physicsEngine.setSpatialAudioEnabled(enabled);
			}

			bool isSpatialAudioEnabled() const
			{
				return m_physicsEngine.isSpatialAudioEnabled();
			}

			// Visualizer data
			const AudioData& getAudioData() const
			{
				return m_visualSnapshot;
			}

			float getAudioVolume() const
			{
				return m_visualSnapshot.currentVolume;
			}

		private:
			AudioEngine();

			WeirdAudio::AudioSettings m_settings;
			PhysicsAudioEngine m_physicsEngine;
			SdfMusicEngine m_musicEngine;

			SDL_AudioStream* m_audioStream = nullptr;
			AudioData m_visualSnapshot;

			// Master bus DC-blocking filter state (per channel)
			float m_dcBlockerX[2] = {0.0f, 0.0f};
			float m_dcBlockerY[2] = {0.0f, 0.0f};
		};

	} // namespace WeirdAudio
} // namespace WeirdEngine
