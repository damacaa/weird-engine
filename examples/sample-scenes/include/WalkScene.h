#pragma once

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace WalkSceneNamespace
{
	struct State
	{
		WeirdEngine::Entity player = WeirdEngine::INVALID_ENTITY;

		// Materials for ground, wall, and air visual feedback
		WeirdEngine::Material2D floorMaterial;
		WeirdEngine::Material2D wallMaterial;
		WeirdEngine::Material2D airMaterial;

		// Platform creation with mouse
		WeirdEngine::Entity previewBox = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::vec2 boxStart{0.0f, 0.0f};
		bool isCreatingBox = false;
		WeirdEngine::CombinationType boxCombination = WeirdEngine::CombinationType::Addition;
		WeirdEngine::Material2D previewAddMaterial;
		WeirdEngine::Material2D previewSubMaterial;

		// Controller tuning
		float moveSpeed = 9.0f;
		float groundAccel = 65.0f;
		float groundDecel = 55.0f;
		float airAccel = 28.0f;
		float airDecel = 15.0f;
		float jumpCutMultiplier = 0.5f;
		float terminalFallVelocity = -25.0f;
		float coyoteTime = 0.12f;
		float jumpBufferTime = 0.10f;

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

		// Runtime state
		float coyoteTimer = 0.0f;
		float jumpBufferTimer = 0.0f;
		float wallJumpLockoutTimer = 0.0f;
		WeirdEngine::vec2 lastContactNormal{0.0f, 1.0f};
		bool isGrounded = false;
		bool isOnWall = false;
		bool wasJumpHeld = false;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void loadCharacterSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupGroundSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void platformBuilderSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void walkingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraFollowSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
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

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(WalkSceneNamespace::platformBuilderSystem);
		addUpdateSystem(WalkSceneNamespace::walkingSystem);
		addUpdateSystem(WalkSceneNamespace::cameraFollowSystem);
		addUpdateSystem(GlobalSystems::cameraTrackingSystem);
	}
};
