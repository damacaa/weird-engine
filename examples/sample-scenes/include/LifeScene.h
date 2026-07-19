#pragma once

#include <weird-engine.h>

#include <filesystem>

#include <vector>
#include "weird-physics/components/GlobalPhysicsSettings.h"

#include "globals.h"

using namespace WeirdEngine;

struct Head
{
	Head() {};

	vec2 direction = vec2(0.0f, 1.0f);
	float forceMagnitude = 10.0f;
	bool directionChanged = false;
};

class LifeScene : public Scene2D
{
public:
	LifeScene(){};

private:
	// Inherited via Scene
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		Entity globalSettingsEnt = ecs.createEntity();
		auto& settings = ecs.addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = 0.0f;
		settings.damping = 0.1f;
		ecs.setComponentDirty(settings);

		const std::filesystem::path organismsDir(ASSETS_PATH "Organisms");
		{
			int i = 0;

			for (const auto& entry : std::filesystem::directory_iterator(organismsDir))
			{
				Logger::log(entry.path());

				if (!entry.is_regular_file() || entry.path().extension() != ".weird")
					continue;

				for (size_t j = 0; j < 3; j++)
				{
					Entity firstCreated = static_cast<Entity>(ecs.getEntityCount());

					auto tags = loadWeirdFile(entry.path().string());

					Entity lastCreated = static_cast<Entity>(ecs.getEntityCount());

					for (Entity e = 0; e < (lastCreated - firstCreated); e++)
					{
						if (!ecs.hasComponent<Transform>(firstCreated + e))
						{
							continue;
						}

						auto& t = ecs.getComponent<Transform>(firstCreated + e);
						t.position += vec3(-10.0f + (float)(i * 10), -10.0f + (float)(j * 10), 0.0f);
					}

					if (tags.contains("head"))
					{
						Entity headEntity = tags["head"];
						ecs.addComponent<Head>(headEntity);
					}
					else
					{
						auto& a = ecs.getComponent<Dot>(firstCreated);
						ecs.addComponent<Head>(firstCreated);
					}

					++i;
					// break;
				}
			}
		}

			ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
		}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}

		updatePhysics(delta, ecs);
	}

	void updatePhysics(float delta, ECSManager& ecs)
	{
		float timeDelta = std::sin(getTime() * 10.0f) * 0.5f + 0.25f;

		auto headArray = ecs.getComponentArray<Head>();

		for (size_t i = 0; i < headArray->getSize(); i++)
		{
			auto& head = headArray->getDataAtIdx(i);
			Entity headEntity = headArray->getEntityAtIdx(i);

			auto& rb = ecs.getComponent<RigidBody2D>(headEntity);
			rb.pendingImpulseForce += head.forceMagnitude * head.direction * timeDelta;

			if (timeDelta < 0.0f && !head.directionChanged)
			{ // rotate direction by random amount between -45 and 45 degrees
				constexpr float MAX_ANGLE = 45.0f;
				float angle = (rand() / (float)RAND_MAX) * MAX_ANGLE - (MAX_ANGLE / 2.0f);
				float radians = angle * 3.14159265f / 180.0f;
				float cosA = cos(radians);
				float sinA = sin(radians);
				head.direction = vec2(cosA * head.direction.x - sinA * head.direction.y,
									  sinA * head.direction.x + cosA * head.direction.y);
				head.direction = normalize(head.direction);
				head.directionChanged = true;
			}
			else
			{
				head.directionChanged = false;
			}

			vec2 positon = vec2(ecs.getComponent<Transform>(headEntity).position);
			if (length(positon) > 50.0f)
			{
				head.direction = -normalize(positon);
			}
		}
	}
};
