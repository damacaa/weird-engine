#pragma once

#include "weird-engine/ecs/ECS.h"

namespace WeirdEngine
{
	struct Spring
	{
		Spring()
			: entityA(INVALID_ENTITY)
			, entityB(INVALID_ENTITY)
			, stiffness(1.0f)
			, restDistance(1.0f) {};

		Entity entityA;
		Entity entityB;
		float stiffness;
		float restDistance;
	};
} // namespace WeirdEngine
