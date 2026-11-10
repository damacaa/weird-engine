#pragma once

#include "GlobalSystems.h"
#include <array>
#include <weird-engine.h>

namespace ThrustSceneNamespace
{
	constexpr size_t NUM_KINEMATIC_BODIES = 7;
	constexpr size_t MAX_EXHAUST_PARTICLES = 1024;

	struct ExhaustParticle
	{
		WeirdEngine::Entity entity = WeirdEngine::INVALID_ENTITY;
		float lifetime = 0.0f;
	};

	struct State
	{
		// Ship visuals & physics entities
		WeirdEngine::Entity shipShapeEntity = WeirdEngine::INVALID_ENTITY;
		std::array<WeirdEngine::Entity, NUM_KINEMATIC_BODIES> shipKinematicBodies = {
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::vec2, NUM_KINEMATIC_BODIES> shipKinematicOffsets = {
			WeirdEngine::vec2(0.0f, 3.0f),	   // Nose / apex (scaled 3x)
			WeirdEngine::vec2(-1.75f, -1.45f), // Left rear wing tip
			WeirdEngine::vec2(1.75f, -1.45f),  // Right rear wing tip
			WeirdEngine::vec2(-0.9f, 0.8f),	   // Left mid hull
			WeirdEngine::vec2(0.9f, 0.8f),	   // Right mid hull
			WeirdEngine::vec2(0.0f, -1.45f),   // Rear thruster base
			WeirdEngine::vec2(0.0f, 0.0f)	   // Ship center
		};

		// UI entities
		WeirdEngine::Entity fuelLineEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity fuelBgLineEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity fuelLabelEntity = WeirdEngine::INVALID_ENTITY;

		// Materials
		WeirdEngine::Material2DHandle shipMaterial;
		WeirdEngine::Material2DHandle exhaustMaterial;
		WeirdEngine::Material2DHandle fuelMaterial;
		WeirdEngine::Material2DHandle fuelBgMaterial;

		// Ship motion
		WeirdEngine::vec2 shipPos{0.0f, 6.0f};
		WeirdEngine::vec2 shipVelocity{0.0f, 0.0f};
		float shipAngle = 0.0f; // 0 radians = pointing UP (+Y)

		// Ship specs & tuning
		float shipWidth = 3.9f;
		float shipHeight = 5.1f;
		float thrustPower = 32.0f;
		float rotationSpeed = 2.3f;
		float gravity = 14.0f;
		float linearDrag = 0.24f;

		// Fuel
		float maxFuel = 100.0f;
		float fuel = 100.0f;
		float fuelBurnRate = 12.0f; // ~8.3 seconds of continuous burn

		// Exhaust particle ring buffer
		std::array<ExhaustParticle, MAX_EXHAUST_PARTICLES> exhaustParticles{};
		size_t exhaustHead = 0;

		// Runtime status
		bool isThrusting = false;
	};

	static_assert(WeirdEngine::is_snapshot_serializable_v<State>,
				  "ThrustScene State must contain only snapshot-compatible values");

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void loadShipSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupUiSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void onShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event);
	void shipPhysicsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void exhaustParticleSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void uiSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraFollowSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace ThrustSceneNamespace

class ThrustScene : public WeirdEngine::Scene2D
{
public:
	ThrustScene()
	{
		addStartSystem(ThrustSceneNamespace::stateInitSystem);
		addStartSystem(ThrustSceneNamespace::setupEnvironmentSystem);
		addStartSystem(ThrustSceneNamespace::loadShipSystem);
		addStartSystem(ThrustSceneNamespace::setupUiSystem);

		addEntityShapeCollisionSystem(ThrustSceneNamespace::onShapeCollisionSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(ThrustSceneNamespace::shipPhysicsSystem);
		addUpdateSystem(ThrustSceneNamespace::exhaustParticleSystem);
		addUpdateSystem(ThrustSceneNamespace::uiSystem);
		addUpdateSystem(ThrustSceneNamespace::cameraFollowSystem);
	}
};
