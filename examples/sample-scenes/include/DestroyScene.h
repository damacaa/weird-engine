#pragma once

#include <weird-engine.h>

#include "globals.h"
#include "weird-physics/components/DistanceConstraint.h"

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
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		spawnNextBatch(ecs);

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

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
						ecs.destroyEntity(e); // Destroy all test balls
					}

					ecs.destroyEntity(m_testShape); // Destroy the test shape
					spawnNextBatch(ecs);
				}
			}
		}
	}

	void spawnNextBatch(ECSManager& ecs)
	{
		for (int i = 0; i < 8; ++i)
		{
			Entity e = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(e);
			t.position = vec3((i + 1) * 3, 20.0f, 0.0f);
			auto& ui = ecs.addComponent<Dot>(e);
			ui.materialId = 4 + (e % 12);
			auto& rb = ecs.addComponent<RigidBody2D>(e);

			if (i > 0)
			{
				Entity constraintEnt = ecs.createEntity();
				auto& constraint = ecs.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
				constraint.entityA = m_testBalls.back();
				constraint.entityB = e;
				constraint.distance = 3.0f;
			}

			m_testBalls.push_back(e);
		}

		float y = -5.0f + (static_cast<int>(std::floor(getTime() * 12345.6789f)) % 10);
		WeirdEngine::Logger::log("Spawning shape at y = " + std::to_string(y));
		float variables[4]{15, y, 15, 5};
		m_testShape = addShape(DefaultShapes::BOX, variables, 2, CombinationType::Addition);
	}

	void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		event.raw.friction *= 100.0f;

		m_collisionDetected = true;
	}
};
