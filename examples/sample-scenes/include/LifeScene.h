#pragma once

#include <weird-engine.h>

namespace LifeSceneNamespace
{
	struct Head
	{
		WeirdEngine::vec2 direction = WeirdEngine::vec2(0.0f, 1.0f);
		float forceMagnitude = 500.0f;
		float phaseOffset = 0.0f;
		float swimFrequency = 10.0f;
		bool directionChanged = false;
	};

	constexpr float MAP_RADIUS = 50.0f;
	constexpr float INNER_BOUNDARY_RADIUS = 54.0f;
	constexpr float OUTER_BOUNDARY_RADIUS = 20000.0f;
	constexpr float SPAWN_RADIUS = 38.0f;
	constexpr float MIN_SPAWN_DIST = 10.0f;
	constexpr float MIN_SPAWN_DIST_SQ = MIN_SPAWN_DIST * MIN_SPAWN_DIST;
	constexpr size_t COPIES_PER_FILE = 3;

	struct State
	{
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupArenaSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnOrganismsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void organismMovementSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace LifeSceneNamespace

class LifeScene : public WeirdEngine::Scene2D
{
public:
	LifeScene()
	{
		addStartSystem(LifeSceneNamespace::stateInitSystem);
		addStartSystem(LifeSceneNamespace::setupArenaSystem);
		addStartSystem(LifeSceneNamespace::spawnOrganismsSystem);
		addStartSystem(LifeSceneNamespace::cameraInitSystem);

		addUpdateSystem(LifeSceneNamespace::sceneControlSystem);
		addUpdateSystem(LifeSceneNamespace::organismMovementSystem);
		addUpdateSystem(LifeSceneNamespace::cameraTrackingSystem);
	}
};
