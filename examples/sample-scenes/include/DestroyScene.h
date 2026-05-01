#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
class DestroyScene : public Scene
{
public:
	DestroyScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:
	std::vector<Entity> m_testBalls;
	Entity m_testShape;

	std::atomic<bool> m_collisionDetected = false;

	float m_countDown = -1.0f;

	// Inherited via Scene
	void onStart() override
	{
		m_debugInput = true;
		m_debugFly = true;

		spawnNextBatch();

		m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta) override
	{
		g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		if (m_collisionDetected)
		{
			if (m_countDown < 0.0f)
			{
				m_countDown = 1.0f; // Start a 3 second countdown when the first collision is detected
			}
			else
			{
				m_countDown -= delta; // Decrease the countdown timer

				if (m_countDown < 0.0f)
				{
					m_collisionDetected = false;
					for (auto e : m_testBalls)
					{
						m_ecs.destroyEntity(e); // Destroy all test balls
					}

					m_ecs.destroyEntity(m_testShape); // Destroy the test shape
					spawnNextBatch();
				}
			}
		}
	}

	void spawnNextBatch()
	{
		for (int i = 0; i < 8; ++i)
		{
			Entity e = m_ecs.createEntity();
			auto& t = m_ecs.addComponent<Transform>(e);
			t.position = vec3((i + 1) * 3, 20.0f, 0.0f);
			auto& ui = m_ecs.addComponent<Dot>(e);
			ui.materialId = 4 + (e % 12);
			auto& rb = m_ecs.addComponent<RigidBody2D>(e);

			if (i > 0)
				m_simulation2D.addPositionConstraint(rb.simulationId, rb.simulationId - 1, 3.0f);

			m_testBalls.push_back(e);
		}

		float y = -5.0f + (static_cast<int>(std::floor(getTime() * 12345.6789f)) % 10);
		std::cout << "Spawning shape at y = " << y << std::endl;
		float variables[4]{15, y, 15, 5};
		m_testShape = addShape(DefaultShapes::BOX, variables, 2, CombinationType::Addition);
	}

	void onEntityShapeCollision(WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		event.raw.friction *= 100.0f;

		m_collisionDetected = true;
	}
};
