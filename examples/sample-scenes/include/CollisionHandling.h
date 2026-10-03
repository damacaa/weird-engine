#pragma once

#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace CollisionHandlingNamespace
{
	struct State
	{
		WeirdEngine::Entity hudText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Material2DHandle ballMat;
		WeirdEngine::Material2DHandle hitMat;
		WeirdEngine::Material2DHandle barrierMat;

		float respawnTimer = 0.0f;
		int ecsCollisionCount = 0;
		std::vector<WeirdEngine::Entity> activeBalls;
	};

	State& getState(WeirdEngine::Registry& registry);
	void spawnLaneBalls(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, State& state);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupArenaShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupHudSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnInitialBallsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void hudSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void onEntityShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
									  WeirdEngine::EntityShapeCollisionEvent& event);
	void onEntityCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								 WeirdEngine::EntityCollisionEvent& event);
} // namespace CollisionHandlingNamespace

class CollisionHandlingScene : public WeirdEngine::Scene2D
{
public:
	CollisionHandlingScene()
	{
		addStartSystem(CollisionHandlingNamespace::stateInitSystem);
		addStartSystem(CollisionHandlingNamespace::setupEnvironmentSystem);
		addStartSystem(CollisionHandlingNamespace::setupMaterialsSystem);
		addStartSystem(CollisionHandlingNamespace::setupArenaShapesSystem);
		addStartSystem(CollisionHandlingNamespace::setupHudSystem);
		addStartSystem(CollisionHandlingNamespace::spawnInitialBallsSystem);
		addStartSystem(GlobalSystems::cameraInitSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(CollisionHandlingNamespace::spawnerSystem);
		addUpdateSystem(CollisionHandlingNamespace::hudSystem);
		addUpdateSystem(GlobalSystems::cameraTrackingSystem);

		addEntityCollisionSystem(CollisionHandlingNamespace::onEntityCollisionSystem);
		addEntityShapeCollisionSystem(CollisionHandlingNamespace::onEntityShapeCollisionSystem);
	}

	void onPhysicsShapeCollision(WeirdEngine::Simulation2D& simulation,
								 WeirdEngine::PhysicsShapeCollisionEvent& event) override;
	void onPhysicsRigidBodyCollision(WeirdEngine::Simulation2D& simulation,
									 WeirdEngine::PhysicsCollisionEvent& event) override;
};
