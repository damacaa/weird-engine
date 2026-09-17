#pragma once

#include "weird-engine/ecs/Registry.h"

namespace WeirdEngine
{
	struct DistanceConstraint
	{
		Entity entityA = INVALID_ENTITY;
		Entity entityB = INVALID_ENTITY;
		float distance = 1.0f;
	};
} // namespace WeirdEngine
