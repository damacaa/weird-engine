#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;

class LifeScene : public Scene
{
public:
	LifeScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:
	Entity m_head;

	inline static int g_id = 0;

	// Inherited via Scene
	void onStart() override
	{
		m_debugInput = true;
		m_debugFly = true;

		m_simulation2D.setGravity(0.0f);
		m_simulation2D.setDamping(0.05f);

		auto& a = m_ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(0);
		m_head = a.Owner;
		g_id++;

		m_ecs.getComponent<Dot>(m_head).materialId = 10;

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

	float m_t = 0.0f;
	vec2 m_direction = vec2(0.0f, 1.0f);
	float m_forceMagnitude = 10.0f;
  bool m_directionChanged = false;
	void onPhysicsStep() override
	{
		float delta = std::sin(getTime() * 10.0f) * 0.5f + 0.25f;

		m_t = getTime();
		auto& rb = m_ecs.getComponent<RigidBody2D>(m_head);
		m_simulation2D.addForce(rb.simulationId, m_forceMagnitude * m_direction * delta);

		if (delta < 0.0f && !m_directionChanged)
		{ // rotate direction by random amount between -45 and 45 degrees
			float angle = (rand() / (float)RAND_MAX) * 90.0f - 45.0f;
			float radians = angle * 3.14159265f / 180.0f;
			float cosA = cos(radians);
			float sinA = sin(radians);
			m_direction =
				vec2(cosA * m_direction.x - sinA * m_direction.y, sinA * m_direction.x + cosA * m_direction.y);
			m_direction = normalize(m_direction);
			m_directionChanged = true;
		}
		else
		{
			m_directionChanged = false;
		}
	}
};
