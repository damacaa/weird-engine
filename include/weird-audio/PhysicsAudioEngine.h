#pragma once

#include "weird-audio/FrictionSource.h"
#include "weird-audio/SimpleAudioRequest.h"
#include "weird-audio/SpatialAudioProcessor.h"
#include "weird-engine/vec.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdAudio
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
			bool releasing = false;
			float releaseRemaining = 0.0f;
			static constexpr float STEAL_RELEASE_SECONDS = 0.005f; // 5 ms soft de-click fade-out
		};

		// Per-source spatial targets for a continuous friction voice.
		struct FrictionSpatialState
		{
			float distanceGain = 1.0f;
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 20000.0f;
		};

		// Smoothed render state of a friction voice. Spatial targets can jump
		// when the source changes between frames (many sliding bodies), so
		// distance gain, pan, and cutoff are all one-pole smoothed.
		struct FrictionVoiceState
		{
			float filterState = 0.0f;
			float smoothedGain = 0.0f;
			float smoothedDistanceGain = 1.0f;
			float smoothedLeftGain = 0.7071f;
			float smoothedRightGain = 0.7071f;
			float smoothedFilterCutoff = 20000.0f;
		};

		// One pooled continuous friction voice. targetLevel is post-compression,
		// smoothedLevel is one-pole smoothed, render holds the audio-rate state.
		// A voice belongs to one spatial cell, so its identity is the cell key.
		struct FrictionVoice
		{
			int32_t cellX = 0;
			int32_t cellY = 0;
			bool active = false;
			uint32_t lastSeenFrame = 0;
			vec3 position = vec3(0.0f);
			float targetLevel = 0.0f;
			float smoothedLevel = 0.0f;
			FrictionSpatialState spatial;
			FrictionVoiceState render;
			float silentSeconds = 0.0f;
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

			// Collision/impact voice pool toggle (gates all playVoice calls)
			void setVoicesEnabled(bool enabled)
			{
				m_voicesEnabled.store(enabled, std::memory_order_release);
			}

			bool isVoicesEnabled() const
			{
				return m_voicesEnabled.load(std::memory_order_acquire);
			}

			size_t getActiveVoiceCount() const
			{
				return m_activeVoices.size();
			}

			// Continuous friction pool: the strongest sources by audibility are
			// spatialized independently via spatial cell binning, so friction from
			// many regions can sound at once.
			void setFrictionSources(std::span<const FrictionSource> sources, const vec3& listenerPos,
									const vec3& listenerForward, const vec3& listenerUp);

			// Spatialized single friction source (UI override / direct callers)
			void setFrictionLevel(float level, const vec3& sourcePos, const vec3& listenerPos,
								  const vec3& listenerForward, const vec3& listenerUp);
			float getFrictionLevel() const;

			static constexpr size_t MAX_FRICTION_VOICES = 16;
			void setMaxFrictionVoices(size_t count);
			size_t getMaxFrictionVoices() const
			{
				return m_maxFrictionVoices;
			}
			// Voices currently selected this frame (target > 0).
			size_t getActiveFrictionVoiceCount() const;
			// Voices fading out after losing their cell (release headroom).
			size_t getReleasingFrictionVoiceCount() const;
			// Hard voice replacements during the last setFrictionSources call.
			size_t getLastFrictionStealCount() const
			{
				return m_lastFrictionStealCount;
			}

			// Friction energy within this band (fraction of a cell, 0-0.5) is
			// bilinearly splatted into neighboring cells, so a body crossing a
			// cell boundary hands its energy over continuously.
			static constexpr float FRICTION_CELL_BLEND_BAND = 0.25f;

			// Spatial cell size for friction binning (world units). Larger cells
			// merge more contacts per voice; smaller cells give finer detail.
			static constexpr float DEFAULT_FRICTION_CELL_SIZE = 16.0f;
			static constexpr float MIN_FRICTION_CELL_SIZE = 4.0f;
			static constexpr float MAX_FRICTION_CELL_SIZE = 64.0f;
			void setFrictionCellSize(float size);
			float getFrictionCellSize() const
			{
				return m_frictionCellSize;
			}
			size_t getOccupiedFrictionCellCount() const
			{
				return m_occupiedFrictionCellCount;
			}

			// Trigger explicit physics audio request
			void playSound(const SimpleAudioRequest& request, const vec3& listenerPos, const vec3& listenerForward,
						   const vec3& listenerUp);

			// Render audio frames into buffer (adds to existing buffer content)
			void render(float* buffer, uint32_t frameCount, uint32_t channels);

		private:
			uint32_t m_sampleRate = 44100;
			uint32_t m_channels = 2;
			float m_volume = 0.8f;
			std::atomic<bool> m_spatialEnabled{true};
			std::atomic<bool> m_voicesEnabled{true};

			// Friction voice pool
			std::array<FrictionVoice, MAX_FRICTION_VOICES> m_frictionVoices;
			size_t m_maxFrictionVoices = 8;
			uint32_t m_frictionFrame = 0;
			float m_smoothedActiveFrictionVoices = 0.0f;
			size_t m_lastFrictionStealCount = 0;

			// Coarse spatial grid for friction binning. Open addressing with a
			// frame stamp per slot: slots never need clearing and the table is
			// reused across frames (no per-frame allocation).
			struct FrictionCell
			{
				int32_t cx = 0;
				int32_t cy = 0;
				uint32_t stamp = 0;
				float powerSum = 0.0f;
				vec3 weightedPosition{0.0f};
				float weightSum = 0.0f;
			};
			std::vector<FrictionCell> m_frictionCells;
			std::vector<uint32_t> m_occupiedFrictionCells;
			uint32_t m_frictionCellStamp = 0;
			float m_frictionCellSize = DEFAULT_FRICTION_CELL_SIZE;
			size_t m_occupiedFrictionCellCount = 0;

			// Pink/White noise state (32-bit xorshift PRNG)
			uint32_t m_noiseSeed = 0x12345678;
			float m_pinkB0 = 0.0f, m_pinkB1 = 0.0f, m_pinkB2 = 0.0f;

			// Active voice pool
			static constexpr size_t MAX_PHYSICS_VOICES = 32;
			static constexpr size_t MAX_PHYSICS_BURST_VOICES = MAX_PHYSICS_VOICES + 16;
			std::vector<PhysicsVoice> m_activeVoices;
			float m_smoothedActiveCollisionVoices = 0.0f;
			float m_lastCollisionMixScale = 1.0f;

			static float mapFrictionLevel(float level);
			void playVoice(float freq, float amp, float decaySec, int instrument, float leftGain, float rightGain,
						   float filterCutoff);
			float generateNoiseSample();
			void renderFrictionVoice(FrictionVoice& voice, float mixScale, float dt, float* buffer,
									 uint32_t frameCount);
			size_t findOrInsertFrictionCell(int32_t cx, int32_t cy);
			void growFrictionCellTable();
			void clearFrictionVoices();
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
