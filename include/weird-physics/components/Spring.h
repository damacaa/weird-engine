#pragma once

#include "weird-engine/ecs/Registry.h"

namespace WeirdEngine
{
	struct Spring
	{
		Entity entityA = INVALID_ENTITY;
		Entity entityB = INVALID_ENTITY;
		float stiffness = 1.0f;
		float restDistance = 1.0f;
	};
} // namespace WeirdEngine
