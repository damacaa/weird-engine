#pragma once

#include "weird-engine/ecs/Component.h"
#include "weird-engine/ecs/ECS.h"

namespace WeirdEngine
{
	struct DistanceConstraint : public Component
	{
		DistanceConstraint()
			: entityA(INVALID_ENTITY), entityB(INVALID_ENTITY), distance(1.0f), isDirty(true) {};
			
		Entity entityA;
		Entity entityB;
		float distance;
		bool isDirty;
	};
}
