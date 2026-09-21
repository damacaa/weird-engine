#pragma once

#include <vector>

#include <weird-engine.h>

namespace MouseCollisionSceneNamespace
{
	struct CollisionCounter
	{
		int count = 0;
	};

	struct State
	{
		WeirdEngine::Entity cursorShape = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Material2DHandle baseDotMat;
		std::vector<WeirdEngine::Material2DHandle> hitMats;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupBoundariesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnDotsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cursorTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ballEntityCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								   WeirdEngine::EntityCollisionEvent& event);
	void ballShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								  WeirdEngine::EntityShapeCollisionEvent& event);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace MouseCollisionSceneNamespace

class MouseCollisionScene : public WeirdEngine::Scene2D
{
public:
	MouseCollisionScene()
	{
		addStartSystem(MouseCollisionSceneNamespace::stateInitSystem);
		addStartSystem(MouseCollisionSceneNamespace::setupMaterialsSystem);
		addStartSystem(MouseCollisionSceneNamespace::setupBoundariesSystem);
		addStartSystem(MouseCollisionSceneNamespace::spawnDotsSystem);
		addStartSystem(MouseCollisionSceneNamespace::cameraInitSystem);

		addUpdateSystem(MouseCollisionSceneNamespace::sceneControlSystem);
		addUpdateSystem(MouseCollisionSceneNamespace::cursorTrackingSystem);
		addUpdateSystem(MouseCollisionSceneNamespace::cameraTrackingSystem);

		addEntityCollisionSystem(MouseCollisionSceneNamespace::ballEntityCollisionSystem);
		addEntityShapeCollisionSystem(MouseCollisionSceneNamespace::ballShapeCollisionSystem);
	}
};
