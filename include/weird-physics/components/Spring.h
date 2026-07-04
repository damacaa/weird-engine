#pragma once

#include "weird-engine/ecs/Component.h"
#include "weird-engine/ecs/ECS.h"

namespace WeirdEngine
{
	struct Spring : public Component
	{
		Spring()
			: entityA(INVALID_ENTITY), entityB(INVALID_ENTITY), stiffness(1.0f), restDistance(1.0f), isDirty(true) {};
			
		Entity entityA;
		Entity entityB;
		float stiffness;
		float restDistance;
		bool isDirty;
	};
}
