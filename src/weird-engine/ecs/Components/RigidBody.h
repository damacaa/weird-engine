#pragma once
#include "../ECS.h"
#include "../../../weird-physics/Simulation.h"

struct RigidBody : public Component {
private:

public:

	RigidBody() : simulationId(-1) {};

	unsigned int simulationId;

};


// Example Systems
class RBPhysicsSystem : public System {
public:
	void init(ECS& ecs, Simulation& simulation) {

		auto& componentArray = GetManagerArray<RigidBody>();


		for (size_t i = 0; i < componentArray.size; i++)
		{
			RigidBody& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);

			simulation.setPosition(rb.Owner, transform.position);
		}

	}

	void addNewRigidbodiesToSimulation(ECS& ecs, Simulation& simulation) {

		auto& componentArray = GetManagerArray<RigidBody>();
		
		for (size_t i = simulation.getSize(); i < componentArray.size; i++)
		{
			RigidBody& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			simulation.setPosition(rb.simulationId, transform.position);
		}

	}

	void update(ECS& ecs, Simulation& simulation) {

		auto& componentArray = GetManagerArray<RigidBody>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			auto& rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			if (transform.isDirty) {
				// Override simulation transform
				simulation.setPosition(rb.simulationId, transform.position);
				transform.isDirty = false; // TODO: move somewhere else
			}

			simulation.updateTransform(transform, rb.simulationId);
		}
	}
};
