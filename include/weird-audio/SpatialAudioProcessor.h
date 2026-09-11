#pragma once

#include "weird-engine/vec.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtx/norm.hpp>

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		struct SpatialAudioResult
		{
			float distanceGain = 1.0f;
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 20000.0f;
		};

		class SpatialAudioProcessor
		{
		public:
			static SpatialAudioResult process(const vec3& sourcePos, const vec3& listenerPos,
											  const vec3& listenerForward, const vec3& listenerUp,
											  bool spatialEnabled = true, float refDistance = 10.0f,
											  float rolloff = 0.45f, float maxDistance = 250.0f)
			{
				SpatialAudioResult result;

				if (!spatialEnabled)
				{
					// Spatial audio disabled: centered stereo, no distance falloff, crisp full-spectrum cutoff
					result.distanceGain = 1.0f;
					result.leftGain = 0.7071f;
					result.rightGain = 0.7071f;
					result.filterCutoff = 20000.0f;
					return result;
				}

				vec3 toSource = sourcePos - listenerPos;
				float distSq = glm::length2(toSource);

				// Early out if beyond max audible distance
				if (distSq > maxDistance * maxDistance)
				{
					result.distanceGain = 0.0f;
					result.leftGain = 0.0f;
					result.rightGain = 0.0f;
					result.filterCutoff = 20.0f;
					return result;
				}

				float dist = std::sqrt(distSq);
				if (dist < 0.001f)
				{
					result.distanceGain = 1.0f;
					result.leftGain = 0.7071f;
					result.rightGain = 0.7071f;
					result.filterCutoff = 20000.0f;
					return result;
				}

				// 1. Distance attenuation: Smooth inverse distance law
				result.distanceGain = refDistance / (refDistance + rolloff * dist);

				// 2. High-frequency acoustic air absorption
				result.filterCutoff = 20000.0f / (1.0f + 0.012f * dist);

				// 3. Fast equal-power vector panning without trigonometric functions
				vec3 forward =
					glm::length2(listenerForward) > 0.001f ? glm::normalize(listenerForward) : vec3(0.0f, 0.0f, -1.0f);
				vec3 up = glm::length2(listenerUp) > 0.001f ? glm::normalize(listenerUp) : vec3(0.0f, 1.0f, 0.0f);
				vec3 right = glm::cross(forward, up);
				if (glm::length2(right) > 0.001f)
				{
					right = glm::normalize(right);
				}
				else
				{
					right = vec3(1.0f, 0.0f, 0.0f);
				}

				vec3 dir = toSource / dist;
				float pan = std::clamp(glm::dot(dir, right), -1.0f, 1.0f);

				// Equal-power law: gL^2 + gR^2 = 1.0
				result.leftGain = std::sqrt(0.5f * (1.0f - pan));
				result.rightGain = std::sqrt(0.5f * (1.0f + pan));

				return result;
			}
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
