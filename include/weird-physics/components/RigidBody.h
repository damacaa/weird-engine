#pragma once

#include "weird-engine/ecs/Component.h"
#include <glm/glm.hpp>
namespace WeirdEngine
{
	struct RigidBody : public Component
	{
		RigidBody()
			: simulationId(-1) {};
		unsigned int simulationId;
	};

	struct RigidBody2D : public Component
	{
		RigidBody2D()
			: simulationId(-1), velocity(0.0f, 0.0f), pendingForce(0.0f, 0.0f), isFixed(false), isDirty(false) {};
		unsigned int simulationId;
		glm::vec2 velocity;
		glm::vec2 pendingForce;
		bool isFixed;
		bool isDirty;
	};
} // namespace WeirdEngine