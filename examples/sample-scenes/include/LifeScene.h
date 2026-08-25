#pragma once

#include <weird-engine.h>

#include <filesystem>

#include "weird-physics/components/GlobalPhysicsSettings.h"
#include <vector>

#include "globals.h"

using namespace WeirdEngine;

struct Head
{
	Head() {};

	vec2 direction = vec2(0.0f, 1.0f);
	float forceMagnitude = 500.0f;
	bool directionChanged = false;
};

class LifeScene : public Scene2D
{
public:
	LifeScene() {};

private:
	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		Entity globalSettingsEnt = registry.createEntity();
		auto& settings = registry.addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = 0.0f;
		settings.damping = 0.1f;
		registry.setComponentDirty(settings);

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		const std::filesystem::path organismsDir(services.resources().assetPath("Organisms"));
		{
			int i = 0;

			for (const auto& entry : std::filesystem::directory_iterator(organismsDir))
			{
				Logger::log(entry.path().string());

				if (!entry.is_regular_file() || entry.path().extension() != ".weird")
					continue;

				for (size_t j = 0; j < 3; j++)
				{
					Entity firstCreated = static_cast<Entity>(registry.getEntityCount());

					auto tags = services.serialization().loadWeirdFile(entry.path().string());

					Entity lastCreated = static_cast<Entity>(registry.getEntityCount());

					for (Entity e = 0; e < (lastCreated - firstCreated); e++)
					{
						if (!registry.hasComponent<Transform>(firstCreated + e))
						{
							continue;
						}

						auto& t = registry.getComponent<Transform>(firstCreated + e);
						t.position += vec3(-10.0f + (float)(i * 10), -10.0f + (float)(j * 10), 0.0f);
					}

					if (tags.contains("head"))
					{
						Entity headEntity = tags["head"];
						registry.addComponent<Head>(headEntity);
					}
					else
					{
						auto& a = registry.getComponent<Dot>(firstCreated);
						registry.addComponent<Head>(firstCreated);
					}

					// break;
				}

				++i;
			}
		}

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

		updateHeads(delta, registry, services);
	}

	void updateHeads(float delta, Registry& registry, ServiceProvider& services)
	{
		float animationT = std::sin(services.time().time() * 10.0f) * 0.5f + 0.25f;

		auto headArray = registry.getComponentArray<Head>();

		for (size_t i = 0; i < headArray->getSize(); i++)
		{
			auto& head = headArray->getDataAtIdx(i);
			Entity headEntity = headArray->getEntityAtIdx(i);

			auto& rb = registry.getComponent<RigidBody2D>(headEntity);
			rb.pendingContinuousForce += head.forceMagnitude * head.direction * animationT;

			if (animationT < 0.0f && !head.directionChanged)
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

			vec2 positon = vec2(registry.getComponent<Transform>(headEntity).position);
			if (length(positon) > 50.0f)
			{
				head.direction = -normalize(positon);
			}
		}
	}
};
