#pragma once

#include <iostream>
#include <filesystem>
#include <random>

#include <weird-engine.h>

using namespace WeirdEngine;

class CoolScene : public WeirdEngine::Scene
{
public:
	CoolScene()
		: Scene() {};

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

			Transform t;
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Entity entity = m_ecs.createEntity();
			m_ecs.addComponent(entity, t);

			if (i < 100)
			{
			}
			m_ecs.addComponent(entity, SDFRenderer(material));

			RigidBody2D rb(m_simulation2D);
			m_ecs.addComponent(entity, rb);
		}

		// Floor
		{
			Entity floor = m_ecs.createEntity();

			float variables[8]{ 0.5f, 1.5f, -1.0f };
			CustomShape shape(0, variables);
			m_ecs.addComponent(floor, shape);
		}
	}

	void onUpdate() override
	{
	}

	void onRender() override
	{
	}
};


int main()
{
	// Create scenes
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<CoolScene>("splash");

	// Start the engine
	start(sceneManager);
}