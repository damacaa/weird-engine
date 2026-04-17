#pragma once

#include <weird-engine.h>

#include <filesystem>

#include "globals.h"

using namespace WeirdEngine;

struct Foot : public Component
{
	Foot() {};

	vec2 direction = vec2(1.0f, 0.0f);
	vec2 initialPos = vec2(0.0f, 0.0f);
	float forceMagnitude = 1.0f;
	bool directionChanged = false;
	float t = 0.0f;
	bool onFloor = false;
	bool stepStarted = false;
};

class WalkScene : public Scene
{
public:
	WalkScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:

	Entity m_head;

	// Inherited via Scene
	void onStart() override
	{
		m_debugInput = true;
		m_debugFly = true;

		auto tags = loadWeirdFile(ASSETS_PATH "Organisms/man.weird");

		Entity firstCreated = static_cast<Entity>(m_ecs.getEntityCount());

		Entity lastCreated = static_cast<Entity>(m_ecs.getEntityCount());

		for (Entity e = 0; e < (lastCreated - firstCreated); e++)
		{
			auto& t = m_ecs.getComponent<Transform>(firstCreated + e);
			t.position += vec3(-10.0f, 0.0f, 0.0f);
		}

		Entity leftFootEntity = tags["foot_left"];
		m_ecs.addComponent<Foot>(leftFootEntity);

		Entity rightFootEntity = tags["foot_right"];
		m_ecs.addComponent<Foot>(rightFootEntity);

		m_head = tags["head"];


		float boundsVars2[8]{0.0f, -24.0f, 200.0f, 20.0f};
		Entity inside = addShape(DefaultShapes::BOX, boundsVars2, DisplaySettings::LightGray, CombinationType::Addition);

		m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

		m_simulation2D.setGravity(-10.0f);
	}

	void onUpdate(float delta) override
	{
		g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}
	}

	int m_currentFoot = 0;
	bool m_feetTouching = false;
	void onPhysicsStep() override
	{
		auto componentArray = m_ecs.getComponentArray<Foot>();
		auto rigidBodies = m_ecs.getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			auto& foot = componentArray->getDataAtIdx(i);
			auto& rb = rigidBodies->getDataFromEntity(foot.Owner);

			if(i != m_currentFoot)
			{
				if(foot.onFloor)
					m_simulation2D.fix(rb.simulationId);
				continue;
			}

			auto& headRB = m_ecs.getComponent<RigidBody2D>(m_head);
			m_simulation2D.addForce(headRB.simulationId, vec2(0.0f, 1.0f));

			m_simulation2D.unFix(rb.simulationId);

			// Start step
			if (!foot.stepStarted)
			{
				if (foot.onFloor)
				{
					foot.initialPos = m_simulation2D.getPosition(rb.simulationId);
					foot.stepStarted = true;
					foot.t = 0.0f;
					// m_simulation2D.setPosition(rb.simulationId, foot.initialPos + vec2(0.0f, 0.1f));
					m_simulation2D.unFix(rb.simulationId);
				}
			}
			else
			{
				// End step
				if (foot.onFloor && foot.t > 0.1f)
				{
					foot.stepStarted = false;
					m_currentFoot = (m_currentFoot + 1) % componentArray->getSize();
					// std::cout << "Switching foot: " << m_currentFoot << std::endl;
				}
				else
				{
					vec2 f;

					if (foot.t < 0.1f)
					{
						f = (foot.direction + vec2(0.0f, 1.0f)) * foot.forceMagnitude;
					}
					else if (foot.t < 0.25f)
					{
						f = (foot.direction + vec2(0.0f, 0.0f)) * foot.forceMagnitude;
					}
					else
					{
						f = (foot.direction + vec2(0.0f, -10.0f * foot.t)) * foot.forceMagnitude * foot.t;
					}

					if(m_feetTouching)
						f.x = 0.0f;

					m_simulation2D.addForce(rb.simulationId, f);

					// vec2 offset = vec2(-std::sin(foot.t), 1.0f - std::abs(std::cos(2.0f * foot.t)));
					// offset.y *= 0.5f;
					// m_simulation2D.setPosition(rb.simulationId, foot.initialPos + offset);
					foot.t += 0.5f * m_simulation2D.getDeltaTime();
				}
			}
		}
		
		m_feetTouching = false;
	}

	void onEntityCollision(WeirdEngine::EntityCollisionEvent& event) override
	{
		Entity entityA = event.entityA;
		Entity entityB = event.entityB;

		if (m_ecs.hasComponent<Foot>(entityA) && m_ecs.hasComponent<Foot>(entityB))
		{
			m_feetTouching = true;
		}
	}

	void onEntityShapeCollision(WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		Entity entity = event.entity;
		if (m_ecs.hasComponent<Foot>(entity))
		{
			auto& foot = m_ecs.getComponent<Foot>(entity);
			if (event.raw.state == CollisionState::START)
			{
				foot.onFloor = true;
			}
			else if (event.raw.state == CollisionState::END)
			{

				foot.onFloor = false;
			}
		}
	}
};
