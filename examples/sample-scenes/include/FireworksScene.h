#pragma once

#include <weird-engine.h>
#include <random>

using namespace WeirdEngine;
class FireworksScene : public Scene
{
public:
	FireworksScene()
		: Scene() {
	};

private:
	// Inherited via Scene
	void onStart() override
	{
		// Create a random number generator engine
		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 0.5f;
		std::uniform_real_distribution<> distrib(-range, range);

		for (size_t i = 0; i < 60; i++)
		{
			float y = 50 + distrib(gen);
			float x = 15 + distrib(gen);

			int material = 4 + (i % 12);

			float z = 0;

			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			if (i < 100)
			{
			}
			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{ 0.5f, 1.5f, -1.0f };
			addShape(0, variables, 3);
		}
	}

	void onUpdate(float delta) override
	{
	}
};
