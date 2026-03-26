#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;

struct Head : public Component
{
	Head() {};

	vec2 direction = vec2(0.0f, 1.0f);
	float forceMagnitude = 10.0f;
	bool directionChanged = false;
};

class LifeScene : public Scene
{
public:
	LifeScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:


	// Inherited via Scene
	void onStart() override
	{
		m_debugInput = true;
		m_debugFly = true;

		m_simulation2D.setGravity(0.0f);
		m_simulation2D.setDamping(0.05f);

		for (size_t i = 0; i < 3; i++)
		{
			Entity firstCreated = m_ecs.getEntityCount();

			loadWeirdFile(ASSETS_PATH "fish.weird");

			Entity lastCreated = m_ecs.getEntityCount() - 1;

			for (size_t e = 0; e < (lastCreated - firstCreated); e++)
			{
				auto& t = m_ecs.getComponent<Transform>(firstCreated + e);
				t.position += vec3(-10.0f + (float)(i * 10), 0.0f, 0.0f);
			}
			

			
			auto& a = m_ecs.getComponent<Dot>(firstCreated);
			m_ecs.addComponent<Head>(a.Owner);
		}
		
		

		m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta) override
	{
		g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}
	}


	void onPhysicsStep() override
	{
		float delta = std::sin(getTime() * 10.0f) * 0.5f + 0.25f;

		auto headArray = m_ecs.getComponentArray<Head>();

		for (size_t i = 0; i < headArray->getSize(); i++)
		{
			auto& head = headArray->getDataAtIdx(i);
			Entity headEntity = head.Owner;

			auto& rb = m_ecs.getComponent<RigidBody2D>(headEntity);
			m_simulation2D.addForce(rb.simulationId, head.forceMagnitude * head.direction * delta);

			if (delta < 0.0f && !head.directionChanged)
			{ // rotate direction by random amount between -45 and 45 degrees
				constexpr float MAX_ANGLE = 45.0f;
				float angle = (rand() / (float)RAND_MAX) * MAX_ANGLE - (MAX_ANGLE / 2.0f);
				float radians = angle * 3.14159265f / 180.0f;
				float cosA = cos(radians);
				float sinA = sin(radians);
				head.direction =
					vec2(cosA * head.direction.x - sinA * head.direction.y, sinA * head.direction.x + cosA * head.direction.y);
				head.direction = normalize(head.direction);
				head.directionChanged = true;
			}
			else
			{
				head.directionChanged = false;
			}
		}
		

		
	}
};
