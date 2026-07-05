#pragma once

#include <weird-engine.h>
#include <cstdlib>

#include "globals.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;

struct CollisionTracker : public Component
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
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
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
					if (m_testBalls.size() < 100)
					{
						Entity e = ecs.createEntity();
						auto& t = ecs.addComponent<Transform>(e);
						t.position = vec3((std::rand() % 200) - 100.0f, (std::rand() % 100) - 50.0f, 0.0f);
						t.isDirty = true;
						auto& ui = ecs.addComponent<Dot>(e);
						ui.materialId = 4 + (e % 12);
						auto& rb = ecs.addComponent<RigidBody2D>(e);
						m_testBalls.push_back(e);
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
						float variables[4]{w, y, x, h};
						uint16_t material = std::rand() % 16;
						Entity shape = addShape(DefaultShapes::BOX, variables, material, CombinationType::Addition);
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
							Entity constraintEnt = ecs.createEntity();
							if (std::rand() % 2 == 0)
							{
								auto& constraint = ecs.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
								constraint.entityA = m_testBalls[idx1];
								constraint.entityB = m_testBalls[idx2];
								constraint.distance = 3.0f + (std::rand() % 5);
							}
							else
							{
								auto& spring = ecs.addComponent<WeirdEngine::Spring>(constraintEnt);
								spring.entityA = m_testBalls[idx1];
								spring.entityB = m_testBalls[idx2];
								spring.restDistance = 3.0f + (std::rand() % 5);
								spring.stiffness = 5.0f;
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
						ecs.destroyEntity(m_testShapes[idx]);
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
						ecs.destroyEntity(m_testBalls[idx]);
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
						ecs.destroyEntity(m_testConstraints[idx]);
						m_testConstraints[idx] = m_testConstraints.back();
						m_testConstraints.pop_back();
					}
					break;
				}
				}
			}
		}
	}

	void onEntityCollision(ECSManager& ecs, WeirdEngine::EntityCollisionEvent& event) override
	{
		if (std::rand() % 5 != 0) return;

		Entity a = event.entityA;

		if (a != INVALID_ENTITY)
		{
			if (!ecs.hasComponent<CollisionTracker>(a))
				ecs.addComponent<CollisionTracker>(a);
			ecs.getComponent<CollisionTracker>(a).collisionCount++;
		}
		
		playSound({0.02f, 400.0f + (std::rand() % 200), false, vec3(0.0f), 1});
	}

	void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		event.raw.friction *= 100.0f;

		if (std::rand() % 20 == 0)
		{
			Entity e = event.entity;
			if (e != INVALID_ENTITY && ecs.hasComponent<RigidBody2D>(e))
			{
				auto& rb = ecs.getComponent<RigidBody2D>(e);
				rb.isFixed = true;
				rb.isDirty = true;
			}
		}

		if (std::rand() % 10 == 0 && !m_testConstraints.empty())
		{
			int idx = std::rand() % m_testConstraints.size();
			Entity constraint = m_testConstraints[idx];
			if (ecs.hasComponent<WeirdEngine::Spring>(constraint))
			{
				auto& spring = ecs.getComponent<WeirdEngine::Spring>(constraint);
				spring.restDistance = 1.0f + (std::rand() % 10);
			}
		}

		if (std::rand() % 5 == 0)
		{
			playSound({0.02f, 200.0f + (std::rand() % 100), false, vec3(event.raw.position, 0.0f), 1});
		}
	}
};
