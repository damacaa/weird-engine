#include "LifeScene.h"

#include "globals.h"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <vector>

using namespace WeirdEngine;

namespace LifeSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "LifeScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupArenaSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		services.physics().setGravity(0.0f);
		services.physics().setDamping(0.1f);

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		// Circular arena boundary: large outer circle with inner arena circle subtracted
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{DefaultShapes::Circle::POS_X, 0.0f},
												  {DefaultShapes::Circle::POS_Y, 0.0f},
												  {DefaultShapes::Circle::RADIUS, OUTER_BOUNDARY_RADIUS}},
									.material = 0,
									.combination = CombinationType::Addition,
									.hasCollision = true});

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{DefaultShapes::Circle::POS_X, 0.0f},
												  {DefaultShapes::Circle::POS_Y, 0.0f},
												  {DefaultShapes::Circle::RADIUS, INNER_BOUNDARY_RADIUS}},
									.material = 0,
									.combination = CombinationType::Subtraction,
									.hasCollision = true});

		// Frame the circular arena nicely at startup
		registry.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(0.0f, 0.0f, 65.0f);
	}

	void spawnOrganismsSystem(Registry& registry, ServiceProvider& services)
	{
		const std::filesystem::path organismsDir(services.resources().assetPath("Organisms"));
		if (std::filesystem::exists(organismsDir))
		{
			std::vector<std::filesystem::path> organismFiles;
			for (const auto& entry : std::filesystem::directory_iterator(organismsDir))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".weird")
				{
					organismFiles.push_back(entry.path());
				}
			}

			size_t totalOrganisms = organismFiles.size() * COPIES_PER_FILE;

			// Generate well-spaced spawn positions across the circular arena using rejection sampling
			std::vector<vec2> spawnPositions;
			spawnPositions.reserve(totalOrganisms);

			for (size_t i = 0; i < totalOrganisms; ++i)
			{
				vec2 candidatePos(0.0f);
				bool validPos = false;

				for (int attempt = 0; attempt < 100; ++attempt)
				{
					// Uniform random sampling inside a disk
					float r = SPAWN_RADIUS * std::sqrt(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX));
					float theta =
						(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f; // 2 * pi
					candidatePos = vec2(r * std::cos(theta), r * std::sin(theta));

					// Rejection check: ensure minimum distance to all existing spawns
					validPos = true;
					for (const auto& existing : spawnPositions)
					{
						vec2 diff = candidatePos - existing;
						if (diff.x * diff.x + diff.y * diff.y < MIN_SPAWN_DIST_SQ)
						{
							validPos = false;
							break;
						}
					}

					if (validPos)
					{
						break;
					}
				}

				spawnPositions.push_back(candidatePos);
			}

			size_t spawnIdx = 0;
			for (const auto& file : organismFiles)
			{
				for (size_t copy = 0; copy < COPIES_PER_FILE; ++copy)
				{
					const vec2& spawnPos =
						(spawnIdx < spawnPositions.size()) ? spawnPositions[spawnIdx++] : vec2(0.0f, 0.0f);

					Entity firstCreated = static_cast<Entity>(registry.getEntityCount());
					auto tags = services.serialization().loadWeirdFile(file.string());
					Entity lastCreated = static_cast<Entity>(registry.getEntityCount());

					// Resolve head entity: check tags, fallback to "mouth", then first created entity
					Entity headEntity = INVALID_ENTITY;
					if (tags.contains("head"))
					{
						headEntity = tags["head"];
					}
					else if (tags.contains("mouth"))
					{
						headEntity = tags["mouth"];
					}
					else if (firstCreated != lastCreated)
					{
						headEntity = firstCreated;
					}

					// Compute centroid of the loaded creature in file space
					vec2 centroid(0.0f);
					int count = 0;
					for (Entity e = firstCreated; e < lastCreated; ++e)
					{
						if (registry.hasComponent<Transform>(e))
						{
							centroid += vec2(registry.getComponent<Transform>(e).position);
							count++;
						}
					}
					if (count > 0)
					{
						centroid /= static_cast<float>(count);
					}

					// Desired facing direction: radial heading toward center + slight tangential jitter
					vec2 toCenter = -spawnPos;
					float distToCenter = length(toCenter);
					vec2 radialDir = (distToCenter > 0.001f) ? (toCenter / distToCenter) : vec2(0.0f, 1.0f);
					vec2 perpDir(-radialDir.y, radialDir.x);
					float jitter =
						((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) - 0.5f) * 0.8f; // [-0.4, 0.4]
					vec2 randomHeading = normalize(radialDir + perpDir * jitter);
					float targetAngle = std::atan2(randomHeading.y, randomHeading.x);

					// Determine rotation angle to align the creature's head towards the target heading
					float rotAngle = 0.0f;
					if (registry.isEntityValid(headEntity) && registry.hasComponent<Transform>(headEntity))
					{
						vec2 headOffset = vec2(registry.getComponent<Transform>(headEntity).position) - centroid;
						if (length(headOffset) > 0.001f)
						{
							float baseAngle = std::atan2(headOffset.y, headOffset.x);
							rotAngle = targetAngle - baseAngle;
						}
						else
						{
							rotAngle = targetAngle;
						}
					}
					else
					{
						rotAngle = targetAngle;
					}

					float cosA = std::cos(rotAngle);
					float sinA = std::sin(rotAngle);

					// Rotate and reposition each entity relative to its centroid
					for (Entity e = firstCreated; e < lastCreated; ++e)
					{
						if (!registry.hasComponent<Transform>(e))
							continue;

						auto& t = registry.getComponent<Transform>(e);
						vec2 rel = vec2(t.position.x - centroid.x, t.position.y - centroid.y);
						vec2 rotated(cosA * rel.x - sinA * rel.y, sinA * rel.x + cosA * rel.y);

						t.position = vec3(spawnPos.x + rotated.x, spawnPos.y + rotated.y, 0.0f);
						registry.setComponentDirty(t, true);
					}

					// Configure head behavior
					if (registry.isEntityValid(headEntity))
					{
						Head& head = registry.hasComponent<Head>(headEntity) ? registry.getComponent<Head>(headEntity)
																			 : registry.addComponent<Head>(headEntity);

						head.direction = randomHeading;
						head.forceMagnitude = 400.0f + static_cast<float>(std::rand() % 200);
						head.phaseOffset =
							(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 6.2831853f;
						head.swimFrequency =
							8.0f + (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 4.0f;
						head.directionChanged = false;
					}
				}
			}
		}
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void organismMovementSystem(Registry& registry, ServiceProvider& services)
	{
		float currentTime = services.time().time();
		auto headArray = registry.getComponentArray<Head>();

		for (size_t i = 0; i < headArray->getSize(); i++)
		{
			auto& head = headArray->getDataAtIdx(i);
			Entity headEntity = headArray->getEntityAtIdx(i);

			if (!registry.isEntityValid(headEntity) || !registry.hasComponent<RigidBody2D>(headEntity))
				continue;

			float animationT = std::sin(currentTime * head.swimFrequency + head.phaseOffset) * 0.5f + 0.25f;

			auto& rb = registry.getComponent<RigidBody2D>(headEntity);
			rb.pendingContinuousForce += head.forceMagnitude * head.direction * animationT;

			if (animationT < 0.0f && !head.directionChanged)
			{
				constexpr float MAX_ANGLE = 45.0f;
				float angle =
					(static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * MAX_ANGLE - (MAX_ANGLE / 2.0f);
				float radians = angle * 3.14159265f / 180.0f;
				float cosA = std::cos(radians);
				float sinA = std::sin(radians);
				head.direction = vec2(cosA * head.direction.x - sinA * head.direction.y,
									  sinA * head.direction.x + cosA * head.direction.y);
				head.direction = normalize(head.direction);
				head.directionChanged = true;
			}
			else if (animationT >= 0.0f)
			{
				head.directionChanged = false;
			}

			if (registry.hasComponent<Transform>(headEntity))
			{
				vec2 position = vec2(registry.getComponent<Transform>(headEntity).position);
				if (length(position) > MAP_RADIUS)
				{
					head.direction = -normalize(position);
				}
			}
		}
	}

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace LifeSceneNamespace