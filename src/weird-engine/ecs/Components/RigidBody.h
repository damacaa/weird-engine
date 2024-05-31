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
	void init(ECS& ecs, Simulation& simulation, unsigned int offset = 0) {

		for (size_t i = 0; i < offset; i++)
		{
			glm::vec3 initialPos;
			initialPos.y = 0.5f * ((2 * i) + 1) + 20;
			initialPos.x = 3.0f * sin(10.1657873f * i);
			initialPos.z = 3.0f * cos(10.2982364f * i);

			simulation.SetPosition(i, initialPos);
		}


		auto& componentArray = GetManagerArray<RigidBody>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			size_t j = i + offset;

			RigidBody& rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);

			transform.position.y = 0.5f * ((2 * (i + 1)) + 1) + 20;
			transform.position.x = 3.0f * sin(10.1657873f * j);
			transform.position.z = 3.0f * cos(10.2982364f * j);

			simulation.SetPosition(rb.Owner + offset, transform.position);
		}


	}

	void update(ECS& ecs, Simulation& simulation, unsigned int offset = 0) {

		auto& componentArray = GetManagerArray<RigidBody>();

		for (size_t i = 0; i < componentArray.size; i++)
		{
			auto rb = componentArray[i];
			Transform& transform = ecs.getComponent<Transform>(rb.Owner);
			if (transform.isDirty) {
				// Override simulation transform
				simulation.SetPosition(offset + i, transform.position);
				transform.isDirty = false; // TODO: move somewhere else
			}

			simulation.UpdateTransform(transform, offset + i);
		}
	}
};
