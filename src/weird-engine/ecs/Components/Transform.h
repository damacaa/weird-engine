#pragma once
#include "../ECS.h"


// Example Components
struct Transform : public Component {
	float x, y, z;
};

// Example Systems
class MovementSystem : public System {
public:
	void update(ECS& ecs, double delta, double time) {
		for (auto entity : entities) {

			auto& t = ecs.getComponent<Transform>(entity);

			float r = 5.0 + (2.0 * sin(0.32154 * entity));

			t.x = r * sin(entity + time);
			t.y = 3;
			t.z = r * cos(entity + time);
		}
	}
};