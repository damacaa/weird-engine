#pragma once

#include <weird-engine.h>

#include <filesystem>

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

		const std::filesystem::path organismsDir(ASSETS_PATH "Organisms");
		for (size_t j = 0; j < 3; j++)
		{
			int i = 0;
			for (const auto& entry : std::filesystem::directory_iterator(organismsDir))
			{
				if (!entry.is_regular_file() || entry.path().extension() != ".weird")
					continue;

				Entity firstCreated = static_cast<Entity>(m_ecs.getEntityCount());

				auto tags = loadWeirdFile(entry.path().string());

				Entity lastCreated = static_cast<Entity>(m_ecs.getEntityCount());

				for (Entity e = 0; e < (lastCreated - firstCreated); e++)
				{
					auto& t = m_ecs.getComponent<Transform>(firstCreated + e);
					t.position += vec3(-10.0f + (float)(i * 10), -10.0f + (float)(j * 10), 0.0f);
				}

				if (tags.contains("head"))
				{
					Entity headEntity = tags["head"];
					m_ecs.addComponent<Head>(headEntity);
				}
				else
				{
					auto& a = m_ecs.getComponent<Dot>(firstCreated);
					m_ecs.addComponent<Head>(a.Owner);
				}

				++i;
			}
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

			vec2 positon = m_simulation2D.getPosition(rb.simulationId);
			if(length(positon) > 50.0f)
			{
				head.direction = -normalize(positon);
			}
		}
		

		
	}
};
