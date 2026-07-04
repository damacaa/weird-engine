#pragma once

#include "weird-engine/ecs/Component.h"
#include <limits>

namespace WeirdEngine
{
	struct GlobalPhysicsSettings : public Component
	{
		GlobalPhysicsSettings()
			: gravity(0.0f), damping(0.05f), isGravityDirty(true), isDampingDirty(true) {};
			
		float gravity;
		float damping;
		bool isGravityDirty;
		bool isDampingDirty;
	};
}
