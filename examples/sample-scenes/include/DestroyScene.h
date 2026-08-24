#pragma once

#include <cstdlib>
#include <weird-engine.h>

#include "globals.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;

struct CollisionTracker
{
	int collisionCount = 0;
};

class DestroyScene : public Scene2D
{
private:
	std::vector<Entity> m_testBalls;
	std::vector<Entity> m_testConstraints;
	std::vector<Entity> m_testShapes;

	float m_timer = 0.0f;

	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		float delta = services.time().deltaTime();
		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		m_timer += delta;
		if (m_timer > 0.02f)
		{
			m_timer = 0.0f;

			for (int i = 0; i < 5; ++i)
			{
				int action = std::rand() % 6;

				switch (action)
				{
					case 0:
					{
						if (m_testBalls.size() < 1000)
						{
							for (int j = 0; j < 10; ++j)
							{
								Entity e = registry.createEntity();
								auto& t = registry.addComponent<Transform>(e);
								t.position = vec3((std::rand() % 200) - 100.0f, (std::rand() % 100) - 50.0f, 0.0f);
								registry.setComponentDirty(t);
								auto& ui = registry.addComponent<Dot>(e);
								ui.materialId = 4 + (e % 12);
								auto& rb = registry.addComponent<RigidBody2D>(e);
								m_testBalls.push_back(e);
							}
						}
						break;
					}
					case 1:
					{
						if (m_testShapes.size() < 20)
						{
							float x = (std::rand() % 200) - 100.0f;
							float y = (std::rand() % 100) - 50.0f;
							float w = (float)(std::rand() % 4 + 1);
							float h = (float)(std::rand() % 4 + 1);
							uint16_t material = std::rand() % 16;
							Entity shape = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
																	   .variables = {{Primitives::Box::POS_X, x},
																					 {Primitives::Box::POS_Y, y},
																					 {Primitives::Box::SIZE_X, w},
																					 {Primitives::Box::SIZE_Y, h}},
																	   .material = material,
																	   .combination = CombinationType::Addition});
							m_testShapes.push_back(shape);
						}
						break;
					}
					case 2:
					{
						if (m_testBalls.size() >= 2 && m_testConstraints.size() < 50)
						{
							int idx1 = std::rand() % m_testBalls.size();
							int idx2 = std::rand() % m_testBalls.size();
							if (idx1 != idx2)
							{
								Entity constraintEnt = registry.createEntity();
								if (std::rand() % 2 == 0)
								{
									auto& constraint =
										registry.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
									constraint.entityA = m_testBalls[idx1];
									constraint.entityB = m_testBalls[idx2];
									constraint.distance = 3.0f + (std::rand() % 5);
								}
								else
								{
									auto& spring = registry.addComponent<WeirdEngine::Spring>(constraintEnt);
									spring.entityA = m_testBalls[idx1];
									spring.entityB = m_testBalls[idx2];
									spring.restDistance = 3.0f + (std::rand() % 5);
									spring.stiffness = 0.5f;
								}
								m_testConstraints.push_back(constraintEnt);
							}
						}
						break;
					}
					case 3:
					{
						if (!m_testShapes.empty())
						{
							int idx = std::rand() % m_testShapes.size();
							registry.destroyEntity(m_testShapes[idx]);
							m_testShapes[idx] = m_testShapes.back();
							m_testShapes.pop_back();
						}
						break;
					}
					case 4:
					{
						if (!m_testBalls.empty())
						{
							int idx = std::rand() % m_testBalls.size();
							registry.destroyEntity(m_testBalls[idx]);
							m_testBalls[idx] = m_testBalls.back();
							m_testBalls.pop_back();
						}
						break;
					}
					case 5:
					{
						if (!m_testConstraints.empty())
						{
							int idx = std::rand() % m_testConstraints.size();
							registry.destroyEntity(m_testConstraints[idx]);
							m_testConstraints[idx] = m_testConstraints.back();
							m_testConstraints.pop_back();
						}
						break;
					}
				}
			}
		}
	}

	void onEntityCollision(Registry& registry, ServiceProvider& services,
						   WeirdEngine::EntityCollisionEvent& event) override
	{
		if (std::rand() % 5 != 0)
			return;

		Entity a = event.entityA;

		if (a != INVALID_ENTITY)
		{
			if (!registry.hasComponent<CollisionTracker>(a))
				registry.addComponent<CollisionTracker>(a);
			registry.getComponent<CollisionTracker>(a).collisionCount++;
		}

		services.audio().playSound({0.02f, 400.0f + (std::rand() % 200), false, vec3(0.0f), 1});
	}

	void onEntityShapeCollision(Registry& registry, ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		if (std::rand() % 20 == 0)
		{
			Entity e = event.entity;
			if (e != INVALID_ENTITY && registry.hasComponent<RigidBody2D>(e))
			{
				auto& rb = registry.getComponent<RigidBody2D>(e);
				rb.isFixed = true;
				registry.setComponentDirty(rb);
			}
		}

		if (std::rand() % 10 == 0 && !m_testConstraints.empty())
		{
			int idx = std::rand() % m_testConstraints.size();
			Entity constraint = m_testConstraints[idx];
			if (registry.hasComponent<WeirdEngine::Spring>(constraint))
			{
				auto& spring = registry.getComponent<WeirdEngine::Spring>(constraint);
				spring.restDistance = 1.0f + (std::rand() % 10);
				registry.setComponentDirty(spring);
			}
		}

		if (std::rand() % 5 == 0)
		{
			services.audio().playSound({0.02f, 200.0f + (std::rand() % 100), false, vec3(event.raw.position, 0.0f), 1});
		}
	}
};
