#pragma once

#include "weird-engine/ecs/ECS.h"

namespace WeirdEngine
{
	struct DistanceConstraint
	{
		DistanceConstraint()
			: entityA(INVALID_ENTITY)
			, entityB(INVALID_ENTITY)
			, distance(1.0f) {};

		Entity entityA;
		Entity entityB;
		float distance;
	};
} // namespace WeirdEngine
