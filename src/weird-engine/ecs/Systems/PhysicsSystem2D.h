#pragma once
#pragma once
#include "../ECS.h"
#include "../../../weird-physics/Simulation2D.h"

struct RigidBody2D : public Component {
private:

public:

	RigidBody2D() : simulationId(-1) {};

	unsigned int simulationId;

};


// Example Systems
class PhysicsSystem2D : public System
{
private:
	std::shared_ptr<ComponentManager> m_rbManager;

public:

	PhysicsSystem2D(ECS& ecs) {
		m_rbManager = ecs.getComponentManager<RigidBody2D>();
	}

	void init(ECS& ecs, Simulation2D& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			RigidBody2D& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);

			simulation.setPosition(rb.Owner, glm::vec2(transform.position));
		}
	}

	void addNewRigidbodiesToSimulation(ECS& ecs, Simulation2D& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody2D>();

		for (size_t i = simulation.getSize(); i < componentArray.size; i++)
		{
			RigidBody2D& rb = componentArray[i];
			rb.simulationId = simulation.generateSimulationID();
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
		}
	}

	void update(ECS& ecs, Simulation2D& simulation) {

		auto& componentArray = *m_rbManager->getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			auto& rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			if (transform.isDirty) {
				// Override simulation transform
				simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
				transform.isDirty = false; // TODO: move somewhere else
			}

			simulation.updateTransform(transform, rb.simulationId);
		}
	}
};