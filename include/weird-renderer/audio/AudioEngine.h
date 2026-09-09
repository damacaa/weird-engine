#pragma once

#include <memory>
#include <miniaudio/miniaudio.h>
#include <SDL3/SDL.h>
#include <vector>

#include "weird-engine/Scene.h"
#include "weird-renderer/audio/AudioSettings.h"
#include "weird-renderer/audio/PhysicsAudioEngine.h"
#include "weird-renderer/audio/SdfMusicEngine.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
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

			bool init(const AudioSettings& settings);
			void close();
			void loadSound(const char* filePath);

			void setAudioStream(SDL_AudioStream* stream)
			{
				m_audioStream = stream;
			}

			SDL_AudioStream* getAudioStream() const
			{
				return m_audioStream;
			}

			ma_uint32 getSampleRate() const;
			ma_uint8 getChannels() const;

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
			AudioData getAudioData();
			float getAudioVolume() const
			{
				return m_visualSnapshot.currentVolume;
			}

			// Procedural physics controls (convenience wrappers)
			void setFrictionLevel(float level)
			{
				m_physicsEngine.setFrictionLevel(level);
			}

			void playSineSound(float freq, float amp, float decaySec = 0.3f)
			{
				m_physicsEngine.playVoice(freq, amp, decaySec, 0);
			}

			void playVoice(float freq, float amp, float decaySec = 0.3f, int instrument = 0, float leftGain = 0.7071f,
						   float rightGain = 0.7071f, float filterCutoff = 20000.0f)
			{
				m_physicsEngine.playVoice(freq, amp, decaySec, instrument, leftGain, rightGain, filterCutoff);
			}

		private:
			AudioEngine();

			AudioSettings m_settings;
			PhysicsAudioEngine m_physicsEngine;
			SdfMusicEngine m_musicEngine;

			ma_engine m_engine;
			ma_sound m_sound; // optional miniaudio sound
			bool m_hasSound = false;

			SDL_AudioStream* m_audioStream = nullptr;
			AudioData m_visualSnapshot;
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
