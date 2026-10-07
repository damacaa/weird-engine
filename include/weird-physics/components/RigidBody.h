#pragma once

#include "weird-physics/SimulationID.h"
#include <glm/glm.hpp>

namespace WeirdEngine
{
	enum class BodyType : uint8_t
	{
		Dynamic = 0,
		Kinematic,
		Fixed
	};

	struct RigidBody
	{
		SimulationID simulationId = INVALID_SIMULATION_ID;
	};

	struct RigidBody2D
	{
		SimulationID simulationId = INVALID_SIMULATION_ID;
		float mass = 1.0f;
		glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
		glm::vec2 pendingImpulseForce = glm::vec2(0.0f, 0.0f);
		glm::vec2 pendingContinuousForce = glm::vec2(0.0f, 0.0f);
		BodyType type = BodyType::Dynamic;
		bool isFixed = false;
		bool enableCollision = true;
	};
} // namespace WeirdEngine
