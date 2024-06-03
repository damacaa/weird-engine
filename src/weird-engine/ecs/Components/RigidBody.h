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
		simulation.setSize(componentArray.getSize());

		for (size_t i = 0; i < componentArray.size; i++)
		{
			RigidBody& rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);

			transform.position.y = (0.5f * i) + 10;
			transform.position.x = 3.0f * sin(10.1657873f * i);
			transform.position.z = 3.0f * cos(10.2982364f * i);

			simulation.setPosition(rb.Owner, transform.position);
		}

	}

	void update(ECS& ecs, Simulation& simulation) {

		auto& componentArray = GetManagerArray<RigidBody>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			auto rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			if (transform.isDirty) {
				// Override simulation transform
				simulation.setPosition(i, transform.position);
				transform.isDirty = false; // TODO: move somewhere else
			}

			simulation.updateTransform(transform, i);
		}
	}
};
