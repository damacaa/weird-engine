#pragma once

#include "weird-engine/vec.h"
#include "weird-renderer/audio/SimpleAudioRequest.h"
#include "weird-renderer/audio/SpatialAudioProcessor.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct PhysicsVoice
		{
			float frequency = 440.0f;
			float amplitude = 0.0f;
			float decay = 0.1f;
			float time = 0.0f;
			float phase = 0.0f;
			bool finished = false;
			int instrument = 3; // 0=Sine, 1=Sawtooth, 2=Square, 3=Filtered Noise, 4=Polyphonic Modal
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 20000.0f;
			float filterState = 0.0f;
		};

		class PhysicsAudioEngine
		{
		public:
			PhysicsAudioEngine();
			~PhysicsAudioEngine();

			void init(uint32_t sampleRate = 44100, uint32_t channels = 2);

			// Configuration
			void setSpatialAudioEnabled(bool enabled)
			{
				m_spatialEnabled.store(enabled, std::memory_order_release);
			}

			bool isSpatialAudioEnabled() const
			{
				return m_spatialEnabled.load(std::memory_order_acquire);
			}

			void setVolume(float volume)
			{
				m_volume = volume;
			}

			float getVolume() const
			{
				return m_volume;
			}

			// Continuous friction level (0.0 to 1.0)
			void setFrictionLevel(float level);
			float getFrictionLevel() const;

			// Trigger explicit physics audio request
			void playSound(const SimpleAudioRequest& request, const vec3& listenerPos, const vec3& listenerForward,
						   const vec3& listenerUp);

			// Direct voice playback
			void playVoice(float freq, float amp, float decaySec, int instrument = 3, float leftGain = 0.7071f,
						   float rightGain = 0.7071f, float filterCutoff = 20000.0f);

			// Render audio frames into buffer (adds to existing buffer content)
			void render(float* buffer, uint32_t frameCount, uint32_t channels);

			// Clear all active sounds (e.g. on scene reset)
			void reset();

		private:
			uint32_t m_sampleRate = 44100;
			uint32_t m_channels = 2;
			float m_volume = 0.8f;
			std::atomic<bool> m_spatialEnabled{true};

			// Friction state
			float m_frictionLevel = 0.0f;
			float m_smoothedFriction = 0.0f;
			float m_lastRawFriction = 0.0f;

			// Pink/White noise state (32-bit xorshift PRNG)
			uint32_t m_noiseSeed = 0x12345678;
			float m_pinkB0 = 0.0f, m_pinkB1 = 0.0f, m_pinkB2 = 0.0f;

			// Active voice pool
			static constexpr size_t MAX_PHYSICS_VOICES = 32;
			std::vector<PhysicsVoice> m_activeVoices;

			float generateNoiseSample();
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
