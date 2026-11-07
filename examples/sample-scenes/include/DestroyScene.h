#pragma once

#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace DestroySceneNamespace
{
	struct CollisionTracker
	{
		int collisionCount = 0;
	};

	struct State
	{
		std::array<WeirdEngine::Material2DHandle, 8> materials{};
		float timer = 0.0f;
	};

	static_assert(WeirdEngine::is_snapshot_serializable_v<State>);

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void stressTestSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace DestroySceneNamespace

class DestroyScene : public WeirdEngine::Scene2D
{
public:
	DestroyScene()
	{
		addStartSystem(DestroySceneNamespace::stateInitSystem);
		addStartSystem(DestroySceneNamespace::setupEnvironmentSystem);
		addStartSystem(DestroySceneNamespace::setupMaterialsSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(DestroySceneNamespace::stressTestSystem);
	}
};
