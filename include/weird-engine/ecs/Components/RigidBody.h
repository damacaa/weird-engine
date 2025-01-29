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
class RBPhysicsSystem : public System
{
private:
	std::shared_ptr<ComponentManager> m_rbManager;

public:

	RBPhysicsSystem(ECSManager& ecs) {
		m_rbManager = ecs.getComponentManager<RigidBody>();
	}

	void init(ECSManager& ecs, Simulation& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			RigidBody& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);

			simulation.setPosition(rb.Owner, transform.position);
		}
	}

	void addNewRigidbodiesToSimulation(ECSManager& ecs, Simulation& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody>();

		for (size_t i = simulation.getSize(); i < componentArray.size; i++)
		{
			RigidBody& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			simulation.setPosition(rb.simulationId, transform.position);
		}
	}

	void update(ECSManager& ecs, Simulation& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody>();

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
