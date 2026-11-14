#pragma once

#include "GlobalSystems.h"
#include <array>
#include <weird-engine.h>

namespace ThrustSceneNamespace
{
	constexpr size_t MAX_SHIP_DOTS = 32;
	constexpr size_t MAX_EXHAUST_PARTICLES = 2560;
	constexpr size_t MAX_RECENT_EXHAUST_SPAWNS = 32;

	struct ShipDot
	{
		WeirdEngine::Entity entity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::vec2 localOffset{0.0f, 0.0f};
	};

	struct ExhaustParticle
	{
		WeirdEngine::Entity entity = WeirdEngine::INVALID_ENTITY;
		float lifetime = 0.0f;
	};

	struct RecentExhaustSpawn
	{
		WeirdEngine::vec2 pos{0.0f, 0.0f};
		WeirdEngine::vec2 vel{0.0f, 0.0f};
		float age = 1.0f;
	};

	struct State
	{
		// Ship cluster entities (driven kinematically from shipPos/shipAngle)
		std::array<ShipDot, MAX_SHIP_DOTS> shipDots{};
		size_t shipDotCount = 0;
		float thrusterLocalY = -2.45f;

		// UI entities
		WeirdEngine::Entity fuelLineEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity fuelBgLineEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity fuelLabelEntity = WeirdEngine::INVALID_ENTITY;

		// Materials
		WeirdEngine::Material2DHandle fuelLabelMaterial;
		WeirdEngine::Material2DHandle fuelMaterial;
		WeirdEngine::Material2DHandle fuelBgMaterial;

		// Ship motion
		WeirdEngine::vec2 shipPos{0.0f, 6.0f};
		WeirdEngine::vec2 shipVelocity{0.0f, 0.0f};
		float shipAngle = 0.0f; // 0 radians = pointing UP (+Y)

		// Ship specs & tuning
		float thrustPower = 32.0f;
		float rotationSpeed = 2.3f;
		float gravity = 14.0f;
		float linearDrag = 0.24f;

		// Fuel
		float maxFuel = 100.0f;
		float fuel = 100.0f;
		float fuelBurnRate = 12.0f; // ~8.3 seconds of continuous burn

		// Exhaust particle ring buffer & smooth emitter state
		std::array<ExhaustParticle, MAX_EXHAUST_PARTICLES> exhaustParticles{};
		size_t exhaustHead = 0;
		std::array<RecentExhaustSpawn, MAX_RECENT_EXHAUST_SPAWNS> recentExhaustSpawns{};
		size_t recentExhaustHead = 0;
		float exhaustSpawnAccumulator = 0.0f;
		float exhaustStaggerSign = 1.0f;

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
	void emitExhaustParticles(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
							  const WeirdEngine::vec2& prevShipPos, float prevShipAngle, bool wasThrusting,
							  float delta);
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

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(ThrustSceneNamespace::shipPhysicsSystem);
		addUpdateSystem(ThrustSceneNamespace::exhaustParticleSystem);
		addUpdateSystem(ThrustSceneNamespace::uiSystem);
		addUpdateSystem(ThrustSceneNamespace::cameraFollowSystem);
	}
};
