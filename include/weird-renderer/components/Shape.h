#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>

#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	using ShapeId = std::uint16_t;

	enum class CombinationType : uint16_t
	{
		Addition,
		Subtraction,
		Intersection,
		SmoothAddition,
		SmoothSubtraction,
	};

	struct Shape
	{
		uint16_t distanceFieldId = 0;
		CombinationType combination = CombinationType::Addition;
		float parameters[8] = {0.0f};
		bool hasCollisions = true;
		uint16_t layer = 0;
		int32_t order = 0;
		uint16_t material = 0;
		float smoothFactor = 1.0f;

		static constexpr uint16_t GLOBAL_LAYER = std::numeric_limits<uint16_t>::max();
	};

	struct UIShape
	{
		uint16_t distanceFieldId = 0;
		CombinationType combination = CombinationType::Addition;
		float parameters[8] = {0.0f};
		uint16_t layer = 0;
		int32_t order = 0;
		uint16_t material = 0;
		float smoothFactor = 10.0f;

		static constexpr uint16_t GLOBAL_LAYER = std::numeric_limits<uint16_t>::max();
	};

	// World-space (or UI screen-space) AABB + CSG metadata for one packed 2D shape instance.
	struct Shape2DAABB
	{
		vec2 min = vec2(-std::numeric_limits<float>::infinity());
		vec2 max = vec2(std::numeric_limits<float>::infinity());
		uint16_t layer = 0;
		CombinationType combination = CombinationType::Addition;
		float smoothFactor = 0.0f;
	};

	// Packs the 12-slot array passed to IMathExpression::getValue for one shape
	// evaluation: [0..7] = shape variables, [8] = time, [9..10] = sample point
	// (world position, or screen position for UI shapes), [11] = audio volume.
	// `variables` must point to at least 8 floats.
	inline void packSdfParameters(float (&parameters)[12], const float* variables, float time, vec2 point,
								  float audioVolume)
	{
		std::copy_n(variables, 8, parameters);
		parameters[8] = time;
		parameters[9] = point.x;
		parameters[10] = point.y;
		parameters[11] = audioVolume;
	}

	// Packs the 12-slot array passed to IMathExpression::getValue for one 3D shape
	// evaluation: [0..7] = shape variables, [8] = time, [9..11] = sample point (x, y, z).
	// Audio volume is skipped in 3D evaluation to keep the array at 12 floats.
	inline void packSdfParameters(float (&parameters)[12], const float* variables, float time, vec3 point)
	{
		std::copy_n(variables, 8, parameters);
		parameters[8] = time;
		parameters[9] = point.x;
		parameters[10] = point.y;
		parameters[11] = point.z;
	}

	// Packs the shared prefix for repeated evaluations where the caller varies the
	// sample point per call: [0..7] = parameters, [8] = time, [11] = 0 (analysis
	// must not feed back through the audio volume). Slots [9..10] are left unset
	// on purpose; the caller overwrites them before each getValue call.
	inline void packSdfParameterPrefix(float (&parameters)[12], const float* variables, float time)
	{
		std::copy_n(variables, 8, parameters);
		parameters[8] = time;
		parameters[11] = 0.0f;
	}
} // namespace WeirdEngine
