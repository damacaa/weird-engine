#pragma once

#include <vector>

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

	struct JellyfishComponent
	{
		WeirdEngine::Entity bellShape = WeirdEngine::INVALID_ENTITY;
		std::vector<WeirdEngine::Entity> tentacleSegments;
		float pulsePhase = 0.0f;
		float pulseSpeed = 0.0f;
		WeirdEngine::vec2 direction = WeirdEngine::vec2(0.0f);
		bool directionChanged = false;
	};

	struct Seaweed
	{
		float animationOffset = 0.0f;
	};

	struct EelComponent
	{
		std::vector<WeirdEngine::Entity> segments;
		float phaseOffset = 0.0f;
		float speed = 0.0f;
		WeirdEngine::vec2 direction = WeirdEngine::vec2(0.0f);
		float segmentSpacing = 0.0f;
		int baseMaterial = 0;
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

	State& getState(WeirdEngine::Registry& registry);

	void createJellyfish(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, float x, float y,
						 WeirdEngine::Material2DHandle material, float scale, float phase);
	void createEel(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, float startX, float startY,
				   int segmentCount, float segmentSpacing, WeirdEngine::Material2DHandle material);
	void spawnBubble(WeirdEngine::Registry& registry, State& state);
	void spawnFood(WeirdEngine::Registry& registry, State& state, WeirdEngine::vec2 pos);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupTankShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnCreaturesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void feedingInputSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void bubbleSpawnerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void bubblePhysicsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void jellyfishSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void seaweedSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void eelSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void fishFlockingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void onEntityShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
									  WeirdEngine::EntityShapeCollisionEvent& event);
	void onEntityCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								 WeirdEngine::EntityCollisionEvent& event);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
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
		addStartSystem(AquariumSceneNamespace::cameraInitSystem);

		addUpdateSystem(AquariumSceneNamespace::sceneControlSystem);
		addUpdateSystem(AquariumSceneNamespace::feedingInputSystem);
		addUpdateSystem(AquariumSceneNamespace::bubbleSpawnerSystem);
		addUpdateSystem(AquariumSceneNamespace::bubblePhysicsSystem);
		addUpdateSystem(AquariumSceneNamespace::jellyfishSystem);
		addUpdateSystem(AquariumSceneNamespace::seaweedSystem);
		addUpdateSystem(AquariumSceneNamespace::eelSystem);
		addUpdateSystem(AquariumSceneNamespace::fishFlockingSystem);
		addUpdateSystem(AquariumSceneNamespace::cameraTrackingSystem);

		addEntityCollisionSystem(AquariumSceneNamespace::onEntityCollisionSystem);
		addEntityShapeCollisionSystem(AquariumSceneNamespace::onEntityShapeCollisionSystem);
	}
};
