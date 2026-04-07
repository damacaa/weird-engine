#pragma once
#include "weird-engine/ecs/Component.h"
#include "weird-engine/math/MathExpressions.h"

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

	struct CustomShape : public Component
	{
		uint16_t distanceFieldId = 0;
		CombinationType combination = CombinationType::Addition;
		float parameters[8] = {0.0f};
		bool isDirty = true;
		bool hasCollisions = true;
		uint16_t groupIdx = 0;
		uint16_t material = 0;
		ShapeId simulationId = 0;
		float smoothFactor = 1.0f;

		static constexpr uint16_t GLOBAL_GROUP = std::numeric_limits<uint16_t>::max();
	};

	struct UIShape : public Component
	{
		uint16_t distanceFieldId = 0;
		CombinationType combination = CombinationType::Addition;
		float parameters[8] = {0.0f};
		uint16_t groupIdx = 0;
		uint16_t material = 0;
		float smoothFactor = 10.0f;

		static constexpr uint16_t GLOBAL_GROUP = std::numeric_limits<uint16_t>::max();
	};
} // namespace WeirdEngine
