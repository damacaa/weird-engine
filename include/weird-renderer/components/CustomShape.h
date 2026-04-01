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
		uint16_t distanceFieldId;
		CombinationType combination;
		float parameters[8];
		bool isDirty;
		bool hasCollisions;
		uint16_t groupIdx;
		uint16_t material;
		ShapeId simulationId;
		float smoothFactor = 1.0f;

		CustomShape() : distanceFieldId(0), isDirty(true)
		{
		}

		CustomShape(uint16_t id, float* params) : distanceFieldId(id), isDirty(true)
		{
			std::copy(params, params + 8, parameters);
		}

		static constexpr uint16_t GLOBAL_GROUP = std::numeric_limits<uint16_t>::max();
	};

	struct UIShape : public Component
	{
		uint16_t distanceFieldId;
		CombinationType combination;
		float parameters[8];
		bool isDirty;
		bool hasCollisions;
		uint16_t groupIdx;
		uint16_t material;
		ShapeId simulationId;
		float smoothFactor = 10.0f;

		UIShape() : distanceFieldId(0), isDirty(true)
		{
		}

		UIShape(uint16_t id, float* params) : distanceFieldId(id), isDirty(true)
		{
			std::copy(params, params + 8, parameters);
		}

		static constexpr uint16_t GLOBAL_GROUP = std::numeric_limits<uint16_t>::max();
	};
}
