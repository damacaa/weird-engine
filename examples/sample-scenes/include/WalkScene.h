#pragma once

#include <weird-engine.h>

namespace WalkSceneNamespace
{
	struct Foot
	{
		WeirdEngine::vec2 direction = WeirdEngine::vec2(1.0f, 0.0f);
		WeirdEngine::vec2 initialPos = WeirdEngine::vec2(0.0f, 0.0f);
		float forceMagnitude = 1.0f;
		bool directionChanged = false;
		float t = 0.0f;
		bool onFloor = false;
		bool stepStarted = false;
	};

	struct State
	{
		WeirdEngine::Entity head = WeirdEngine::INVALID_ENTITY;
		int currentFoot = 0;
		bool feetTouching = false;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void loadCharacterSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupGroundSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void walkingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void feetCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
							 WeirdEngine::EntityCollisionEvent& event);
	void floorCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
							  WeirdEngine::EntityShapeCollisionEvent& event);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace WalkSceneNamespace

class WalkScene : public WeirdEngine::Scene2D
{
public:
	WalkScene()
	{
		addStartSystem(WalkSceneNamespace::stateInitSystem);
		addStartSystem(WalkSceneNamespace::setupEnvironmentSystem);
		addStartSystem(WalkSceneNamespace::loadCharacterSystem);
		addStartSystem(WalkSceneNamespace::setupGroundSystem);
		addStartSystem(WalkSceneNamespace::cameraInitSystem);

		addUpdateSystem(WalkSceneNamespace::sceneControlSystem);
		addUpdateSystem(WalkSceneNamespace::walkingSystem);
		addUpdateSystem(WalkSceneNamespace::cameraTrackingSystem);

		addEntityCollisionSystem(WalkSceneNamespace::feetCollisionSystem);
		addEntityShapeCollisionSystem(WalkSceneNamespace::floorCollisionSystem);
	}
};
