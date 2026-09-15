#pragma once

#include <glm/glm.hpp>
namespace WeirdEngine
{
	struct RigidBody
	{
		RigidBody()
			: simulationId(-1) {};
		unsigned int simulationId;
	};

	struct RigidBody2D
	{
		RigidBody2D()
			: simulationId(-1)
			, mass(1.0f)
			, velocity(0.0f, 0.0f)
			, pendingImpulseForce(0.0f, 0.0f)
			, pendingContinuousForce(0.0f, 0.0f)
			, isFixed(false) {};
		unsigned int simulationId;
		float mass;
		glm::vec2 velocity;
		glm::vec2 pendingImpulseForce;
		glm::vec2 pendingContinuousForce;
		bool isFixed;
	};
} // namespace WeirdEngine
