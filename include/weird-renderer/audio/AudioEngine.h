#pragma once

#include <mutex>
#include <vector>

#include <miniaudio/miniaudio.h>
#include <SDL3/SDL.h>

#include "weird-engine/Scene.h"
#include "weird-renderer/audio/AudioSettings.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		struct CollisionVoice
		{
			float frequency;
			float amplitude;
			float decay;
			float time = 0.0f;
			float phase = 0.0f;
			bool finished = false;
		};

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
			static void data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount,
									  int total_amount);

			ma_uint32 getSampleRate() const;
			ma_uint8 getChannels() const;

			void listen(Scene& scene);

			void mute()
			{
				m_mute = true;
			}

			void unmute()
			{
				m_mute = false;
			}

			AudioData getAudioData();

		private:
			AudioEngine();
			ma_engine m_engine;
			ma_sound m_sound; // background music

			bool m_mute;

			// Procedural state
			ma_noise m_noise;
			float m_frictionLevel = 0.0f; // modulated each frame
			float m_smoothedFriction = 0.0f;

			// Collision tone
			float m_collisionFreq = 0.0f;
			float m_collisionAmp = 0.0f;
			float m_collisionPhase = 0.0f;
			float m_collisionDecay = 0.0f;
			float m_collisionTime = 0.0f;

			// Vector of voices
			std::vector<CollisionVoice> m_activeVoices;
			std::mutex m_voiceMutex; // Essential for thread safety

			// Visualizer
			std::mutex m_visualMutex;
			AudioData m_visualSnapshot;

			// Procedural control
			void setFrictionLevel(float level); // 0..1, continuous
			void playSineSound(float freq, float amp, float decaySec = 0.3f);
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
