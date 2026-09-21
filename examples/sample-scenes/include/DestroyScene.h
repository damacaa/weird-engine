#pragma once

#include <vector>

#include <weird-engine.h>

namespace DestroySceneNamespace
{
	struct CollisionTracker
	{
		int collisionCount = 0;
	};

	struct State
	{
		std::vector<WeirdEngine::Entity> testBalls;
		std::vector<WeirdEngine::Entity> testConstraints;
		std::vector<WeirdEngine::Entity> testShapes;
		std::vector<WeirdEngine::Material2DHandle> materials;
		float timer = 0.0f;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void stressTestSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace DestroySceneNamespace

class DestroyScene : public WeirdEngine::Scene2D
{
public:
	DestroyScene()
	{
		addStartSystem(DestroySceneNamespace::stateInitSystem);
		addStartSystem(DestroySceneNamespace::setupEnvironmentSystem);
		addStartSystem(DestroySceneNamespace::setupMaterialsSystem);
		addStartSystem(DestroySceneNamespace::cameraInitSystem);

		addUpdateSystem(DestroySceneNamespace::sceneControlSystem);
		addUpdateSystem(DestroySceneNamespace::stressTestSystem);
		addUpdateSystem(DestroySceneNamespace::cameraTrackingSystem);
	}
};
