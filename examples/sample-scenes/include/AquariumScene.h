#pragma once

#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace AquariumSceneNamespace
{
	struct Fish
	{
		WeirdEngine::vec2 velocity = WeirdEngine::vec2(0.0f);
		float maxSpeed = 0.0f;
		float separationWeight = 0.0f;
		float alignmentWeight = 0.0f;
		float cohesionWeight = 0.0f;
		float perceptionRadius = 0.0f;
		float energy = 0.0f;
		float mateCooldown = 0.0f;
	};

	struct FishFood
	{
		bool eaten = false;
	};

	struct Bubble
	{
		float wobbleSpeed = 4.0f;
		float wobblePhase = 0.0f;
	};

	struct Seaweed
	{
		float animationOffset = 0.0f;
	};

	constexpr float TANK_LEFT = -20.0f;
	constexpr float TANK_RIGHT = 50.0f;
	constexpr float TANK_BOTTOM = -10.0f;
	constexpr float TANK_TOP = 60.0f;
	constexpr float TANK_CX = (TANK_LEFT + TANK_RIGHT) * 0.5f;
	constexpr float TANK_CY = (TANK_BOTTOM + TANK_TOP) * 0.5f;
	constexpr float TANK_W = TANK_RIGHT - TANK_LEFT;
	constexpr float TANK_H = TANK_TOP - TANK_BOTTOM;

	struct State
	{
		float time = 0.0f;
		float bubbleTimer = 0.0f;
		WeirdEngine::Material2DHandle bubbleMat;
		WeirdEngine::Material2DHandle foodMat;
	};

	static_assert(WeirdEngine::is_snapshot_serializable_v<State>);
	static_assert(WeirdEngine::is_snapshot_serializable_v<Fish>);
	static_assert(WeirdEngine::is_snapshot_serializable_v<FishFood>);
	static_assert(WeirdEngine::is_snapshot_serializable_v<Bubble>);
	static_assert(WeirdEngine::is_snapshot_serializable_v<Seaweed>);

	State& getState(WeirdEngine::Registry& registry);

	void spawnBubble(WeirdEngine::Registry& registry, State& state);
	void spawnFood(WeirdEngine::Registry& registry, State& state, WeirdEngine::vec2 pos);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupTankShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnCreaturesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void feedingInputSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void bubbleSpawnerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void bubblePhysicsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void seaweedSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void fishFlockingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void onEntityShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
									  WeirdEngine::EntityShapeCollisionEvent& event);
	void onEntityCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								 WeirdEngine::EntityCollisionEvent& event);
} // namespace AquariumSceneNamespace

class AquariumScene : public WeirdEngine::Scene2D
{
public:
	AquariumScene()
	{
		addStartSystem(AquariumSceneNamespace::stateInitSystem);
		addStartSystem(AquariumSceneNamespace::setupEnvironmentSystem);
		addStartSystem(AquariumSceneNamespace::setupMaterialsSystem);
		addStartSystem(AquariumSceneNamespace::setupTankShapesSystem);
		addStartSystem(AquariumSceneNamespace::spawnCreaturesSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(AquariumSceneNamespace::feedingInputSystem);
		addUpdateSystem(AquariumSceneNamespace::bubbleSpawnerSystem);
		addUpdateSystem(AquariumSceneNamespace::bubblePhysicsSystem);
		addUpdateSystem(AquariumSceneNamespace::seaweedSystem);
		addUpdateSystem(AquariumSceneNamespace::fishFlockingSystem);

		addEntityCollisionSystem(AquariumSceneNamespace::onEntityCollisionSystem);
		addEntityShapeCollisionSystem(AquariumSceneNamespace::onEntityShapeCollisionSystem);
	}
};
