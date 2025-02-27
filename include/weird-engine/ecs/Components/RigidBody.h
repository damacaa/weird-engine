#pragma once
#include "../ECS.h"
#include "../../../weird-physics/Simulation.h"

namespace WeirdEngine
{
	using namespace ECS;

	struct RigidBody : public Component {
	private:

	public:

		RigidBody() : simulationId(-1) {};

		unsigned int simulationId;

	};

	struct RigidBody2D : public Component
	{
	private:
	public:
		RigidBody2D()
			: simulationId(-1) {};
		RigidBody2D(Simulation2D& simulation)
			: simulationId(simulation.generateSimulationID()) {}; // simulation.generateSimulationID()

		unsigned int simulationId;
	};
}