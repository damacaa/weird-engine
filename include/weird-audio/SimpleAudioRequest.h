#pragma once

#include "weird-engine/vec.h"
#include <algorithm>
#include <cstdint>

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		// Dimensionality of a sound source. 2D sources live on the XY plane:
		// the listener's Z (camera zoom in 2D scenes) is ignored and only the
		// planar distance/pan is used. 3D sources use the full listener state.
		enum class AudioSpace : uint8_t
		{
			TwoDimensional,
			ThreeDimensional
		};

		struct SimpleAudioRequest
		{
			// Which engine collision response the impact should mimic.
			enum class ImpactType : uint8_t
			{
				BodyBody, // rigidbody vs rigidbody (crisper, lower peak volume)
				Shape	  // rigidbody vs SDF shape (deeper, louder)
			};

			float volume = 0.5f;
			float frequency = 0.0f; // 0.0f indicates auto-quantize to current active chord
			bool spatial = false;
			vec3 position = vec3(0.0f);
			int beats = 1;
			float intensity = 0.5f; // Impact energy 0.0 (light) to 1.0 (heavy)
			int instrument = 0;		// 0 = Sine, 1 = Sawtooth, 2 = Square, 3 = Noise, 4 = Polyphonic
			AudioSpace space = AudioSpace::TwoDimensional; // Engine physics is planar today

			// Builds a one-shot filtered-noise impact request. This is the single
			// source of truth for collision impact synthesis: Scene's real
			// collisions and AudioService::playCollisionSound both go through it.
			// `volumeScale` is the user-facing collision volume multiplier
			// (1.0 = 100% = 1.5x the historical mix).
			static SimpleAudioRequest makeImpact(const vec3& position, float intensity,
												 ImpactType type = ImpactType::Shape, float volumeScale = 1.0f)
			{
				constexpr float BASE_GAIN = 1.5f;
				const float clamped = std::clamp(intensity, 0.0f, 1.0f);

				SimpleAudioRequest req;
				if (type == ImpactType::BodyBody)
				{
					// Heavier impacts sound lower, lighter sound crisper
					req.frequency = 250.0f + 800.0f * (1.0f - clamped * 0.5f);
					req.volume = std::clamp(clamped * 0.3f, 0.01f, 0.4f) * BASE_GAIN * volumeScale;
				}
				else
				{
					// Heavier impacts have deeper bass, lighter have crisper click
					req.frequency = 180.0f + 1100.0f * (1.0f - clamped * 0.5f);
					req.volume = std::clamp(clamped * 0.35f, 0.01f, 0.5f) * BASE_GAIN * volumeScale;
				}
				req.spatial = true;
				req.position = position;
				req.intensity = clamped;
				req.instrument = 3; // Filtered noise
				req.beats = 1;
				return req;
			}
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
