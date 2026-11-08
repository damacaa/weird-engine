#pragma once

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace PlatformerSceneNamespace
{
	struct State
	{
		WeirdEngine::Entity player = WeirdEngine::INVALID_ENTITY;

		// Materials for ground, wall, and air visual feedback
		WeirdEngine::Material2DHandle floorMaterial;
		WeirdEngine::Material2DHandle wallMaterial;
		WeirdEngine::Material2DHandle airMaterial;

		// Platform creation with mouse
		WeirdEngine::Entity previewBox = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::vec2 boxStart{0.0f, 0.0f};
		bool isCreatingBox = false;
		WeirdEngine::CombinationType boxCombination = WeirdEngine::CombinationType::Addition;
		WeirdEngine::Material2DHandle previewAddMaterial;
		WeirdEngine::Material2DHandle previewSubMaterial;

		// Controller tuning
		float moveSpeed = 9.0f;
		float groundAccel = 65.0f;
		float groundDecel = 55.0f;
		float airAccel = 28.0f;
		float airDecel = 15.0f;
		float gravity = 32.0f;
		float jumpCutMultiplier = 0.5f;
		float terminalFallVelocity = -25.0f;
		float coyoteTime = 0.10f;
		float jumpBufferTime = 0.05f;

		// Surface jump tuning (normal-directed push + upward lift)
		float jumpNormalPush = 11.0f;
		float jumpUpwardLift = 10.0f;
		float maxJumpVelocityY = 14.0f;
		float minJumpVelocityY = 7.0f;
		float maxJumpVelocityX = 12.0f;
		float maxJumpSpeed = 19.0f;
		float maxUpwardVelocity = 14.0f;
		float wallSlideSpeed = 4.0f;
		float wallJumpLockout = 0.18f;

		// Slope tuning
		float minGroundNormalY = 0.40f;	   // Surfaces with normal.y >= this count as ground or slope
		float maxWalkableNormalY = 0.68f;  // Slopes steeper than this (normal.y < 0.68) will slide down
		float slopeSlideMultiplier = 1.1f; // Slide acceleration multiplier on steep slopes
		float maxSlideSpeed = 16.0f;	   // Max terminal slide speed on steep slopes

		// Runtime state
		float coyoteTimer = 0.0f;
		float jumpBufferTimer = 0.0f;
		float jumpGraceTimer = 0.0f;
		float wallJumpLockoutTimer = 0.0f;
		WeirdEngine::vec2 lastContactNormal{0.0f, 1.0f};
		WeirdEngine::vec2 lastAirborneVelocity{0.0f, 0.0f};
		bool isGrounded = false;
		bool isSliding = false;
		bool isOnWall = false;
		bool wasJumpHeld = false;
		bool playedImpactThisFrame = false;
	};
	static_assert(WeirdEngine::is_snapshot_serializable_v<State>,
				  "PlatformerScene State must contain only snapshot-compatible values");

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void loadCharacterSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupGroundSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void onShapeCollisionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event);
	void platformBuilderSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void walkingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraFollowSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace PlatformerSceneNamespace

class PlatformerScene : public WeirdEngine::Scene2D
{
public:
	PlatformerScene()
	{
		addStartSystem(PlatformerSceneNamespace::stateInitSystem);
		addStartSystem(PlatformerSceneNamespace::setupEnvironmentSystem);
		addStartSystem(PlatformerSceneNamespace::loadCharacterSystem);
		addStartSystem(PlatformerSceneNamespace::setupGroundSystem);

		addEntityShapeCollisionSystem(PlatformerSceneNamespace::onShapeCollisionSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(PlatformerSceneNamespace::platformBuilderSystem);
		addUpdateSystem(PlatformerSceneNamespace::walkingSystem);
		addUpdateSystem(PlatformerSceneNamespace::cameraFollowSystem);
	}
};
