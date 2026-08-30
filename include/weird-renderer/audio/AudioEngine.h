#pragma once

#include <memory>
#include <vector>

#include <miniaudio/miniaudio.h>
#include <SDL3/SDL.h>

#include "weird-engine/Scene.h"
#include "weird-renderer/audio/AudioModule.h"
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
			InstrumentType instrument = InstrumentType::Sine;
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 20000.0f;
			float filterState = 0.0f;
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

			void listen(Scene& scene);

			void mute()
			{
				m_mute = true;
			}

			void unmute()
			{
				m_mute = false;
			}

			bool isMuted() const
			{
				return m_mute;
			}

			AudioData getAudioData();

			// Procedural control
			void setFrictionLevel(float level); // 0..1, continuous
			void playSineSound(float freq, float amp, float decaySec = 0.3f);
			void playVoice(float freq, float amp, float decaySec = 0.3f,
						   InstrumentType instrument = InstrumentType::Sine, float leftGain = 0.7071f,
						   float rightGain = 0.7071f, float filterCutoff = 20000.0f);

			// Audio Module support
			AudioModule* createModule()
			{
				return createProceduralMusicGenerator();
			}

			void setModule(AudioModule* module)
			{
				m_audioModule = module;
			}

			AudioModule* getModule() const
			{
				return m_audioModule;
			}

		private:
			AudioEngine();
			ma_engine m_engine;
			ma_sound m_sound; // background music

			bool m_mute = false;
			bool m_enableAmbient = true;

			SDL_AudioStream* m_audioStream = nullptr;

			// Procedural state (legacy - kept for backward compatibility)
			ma_noise m_noise;
			float m_frictionLevel = 0.0f; // modulated each frame
			float m_smoothedFriction = 0.0f;
			float m_lastRawFriction = 0.0f;

			// Collision tone
			float m_collisionFreq = 0.0f;
			float m_collisionAmp = 0.0f;
			float m_collisionPhase = 0.0f;
			float m_collisionDecay = 0.0f;
			float m_collisionTime = 0.0f;

			// Vector of voices
			std::vector<CollisionVoice> m_activeVoices;

			// Visualizer
			AudioData m_visualSnapshot;

			// Audio Module (new modular system)
			AudioModule* m_audioModule = nullptr;
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
