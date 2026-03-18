#pragma once

#include "weird-engine/ecs/Component.h"

namespace WeirdEngine
{
	struct RigidBody : public Component
	{
		RigidBody() : simulationId(-1) {};
		unsigned int simulationId;
	};

	struct RigidBody2D : public Component
	{
		RigidBody2D()
			: simulationId(-1) {};
		unsigned int simulationId;
	};
}