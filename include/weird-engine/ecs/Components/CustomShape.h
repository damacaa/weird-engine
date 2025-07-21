#pragma once
#include "../Component.h"
#include "../../math/MathExpressions.h"

namespace WeirdEngine
{
	using ShapeId = std::uint16_t;

	enum class CombinationType : uint16_t
	{
		Addition,
		Subtraction,
		Intersection,
		SmoothAddition
	};

	struct CustomShape : public Component
	{
	public:

		uint16_t m_distanceFieldId;
		CombinationType m_combination;
		float m_parameters[8];
		bool m_isDirty;
		bool m_screenSpace;
		bool m_hasCollision;
		uint16_t m_groupId;
		uint16_t m_material;

		CustomShape() : m_distanceFieldId(0), m_isDirty(true), m_screenSpace(false)
		{
		}

		CustomShape(uint16_t id, float* params) : m_distanceFieldId(id), m_isDirty(true)
		{
			std::copy(params, params + 8, m_parameters);
		}

		static constexpr ShapeId CIRCLE = 2;
		static constexpr ShapeId BOX = 3;
		static constexpr ShapeId SINE = 0;
		static constexpr ShapeId STAR = 1;

		static constexpr uint16_t GLOBAL_GROUP = std::numeric_limits<uint16_t>::max();

	};
}
