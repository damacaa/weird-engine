#pragma once

#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace CollisionSceneNamespace
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
	void cursorTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ballEntityCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								   WeirdEngine::EntityCollisionEvent& event);
	void ballShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								  WeirdEngine::EntityShapeCollisionEvent& event);
} // namespace CollisionSceneNamespace

class CollisionScene : public WeirdEngine::Scene2D
{
public:
	CollisionScene()
	{
		addStartSystem(CollisionSceneNamespace::stateInitSystem);
		addStartSystem(CollisionSceneNamespace::setupMaterialsSystem);
		addStartSystem(CollisionSceneNamespace::setupBoundariesSystem);
		addStartSystem(CollisionSceneNamespace::spawnDotsSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(CollisionSceneNamespace::cursorTrackingSystem);

		addEntityCollisionSystem(CollisionSceneNamespace::ballEntityCollisionSystem);
		addEntityShapeCollisionSystem(CollisionSceneNamespace::ballShapeCollisionSystem);
	}
};
