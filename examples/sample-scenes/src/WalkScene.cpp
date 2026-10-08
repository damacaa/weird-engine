#include "WalkScene.h"

#include <algorithm>
#include <cmath>

using namespace WeirdEngine;

namespace WalkSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "WalkScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Sky;
		background.primaryColor = vec4(0.2f, 0.55f, 0.9f, 1.0f);
		background.secondaryColor = vec4(0.4f, 0.75f, 0.85f, 1.0f);
		background.scale = 0.2f;

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		services.physics().setGravity(-25.0f);
		services.physics().setDamping(0.0f);
		services.physics().setFriction(0.0f);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 4.0f, 25.0f);
	}

	void loadCharacterSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		state.floorMaterial = services.materials2D().createMaterial("player_floor");
		state.floorMaterial.color = ColorPalette::Yellow; // Yellow on floor

		state.wallMaterial = services.materials2D().createMaterial("player_wall");
		state.wallMaterial.color = ColorPalette::Orange; // Orange on wall

		state.airMaterial = services.materials2D().createMaterial("player_air");
		state.airMaterial.color = ColorPalette::Magenta; // Magenta in air

		Entity player = registry.createEntity();
		auto& t = registry.addComponent<Transform>(player);
		t.position = vec3(0.0f, 4.0f, 0.0f);

		auto& dot = registry.addComponent<Dot>(player);
		dot.materialId = state.airMaterial.id;

		auto& rb = registry.addComponent<RigidBody2D>(player);
		rb.mass = 1.0f;
		rb.type = BodyType::Kinematic;
		rb.enableCollision = true;

		state.player = player;
	}

	void setupGroundSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& groundMat = services.materials2D().createMaterial("ground");
		groundMat.color = ColorPalette::LightGreen;

		auto& platformMat = services.materials2D().createMaterial("platform");
		platformMat.color = ColorPalette::Cyan;

		state.previewAddMaterial = services.materials2D().createMaterial("ui_box_preview_add");
		state.previewAddMaterial.color = ColorPalette::White;

		state.previewSubMaterial = services.materials2D().createMaterial("ui_box_preview_sub");
		state.previewSubMaterial.color = ColorPalette::Red;

		// Main floor: flat ground at y = 0
		services.shapes().addShape({.shapeId = DefaultShapes::SineWave,
									.variables = {{DefaultShapes::SineWave::Amplitude, 0.0f},
												  {DefaultShapes::SineWave::Period, 0.0f},
												  {DefaultShapes::SineWave::Speed, 0.0f},
												  {DefaultShapes::SineWave::Offset, 0.0f}},
									.material = groundMat,
									.combination = CombinationType::Addition});

		// Floating platforms to jump on and walk over
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 7.0f},
												  {DefaultShapes::Box::PosY, 2.5f},
												  {DefaultShapes::Box::SizeX, 3.0f},
												  {DefaultShapes::Box::SizeY, 0.3f}},
									.material = platformMat,
									.combination = CombinationType::Addition});

		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 15.0f},
												  {DefaultShapes::Box::PosY, 5.0f},
												  {DefaultShapes::Box::SizeX, 3.0f},
												  {DefaultShapes::Box::SizeY, 0.3f}},
									.material = platformMat,
									.combination = CombinationType::Addition});

		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, -7.0f},
												  {DefaultShapes::Box::PosY, 3.0f},
												  {DefaultShapes::Box::SizeX, 3.0f},
												  {DefaultShapes::Box::SizeY, 0.3f}},
									.material = platformMat,
									.combination = CombinationType::Addition});

		// Vertical pillar to test wall jumping
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, -11.0f},
												  {DefaultShapes::Box::PosY, 5.0f},
												  {DefaultShapes::Box::SizeX, 0.8f},
												  {DefaultShapes::Box::SizeY, 5.0f}},
									.material = platformMat,
									.combination = CombinationType::Addition});

		// UI preview box line cursor for platform creation (initially offscreen)
		state.previewBox = services.shapes().addUIShape({.shapeId = DefaultShapes::BoxLine,
														 .variables = {{DefaultShapes::BoxLine::PosX, -1000.0f},
																	   {DefaultShapes::BoxLine::PosY, -1000.0f},
																	   {DefaultShapes::BoxLine::SizeX, 0.0f},
																	   {DefaultShapes::BoxLine::SizeY, 0.0f},
																	   {DefaultShapes::BoxLine::Thickness, 2.0f}},
														 .material = state.previewAddMaterial});
	}

	void platformBuilderSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (!registry.isEntityValid(state.previewBox) || !registry.hasComponent<UIShape>(state.previewBox))
		{
			return;
		}

		auto& previewUi = registry.getComponent<UIShape>(state.previewBox);

		if (!state.isCreatingBox)
		{
			if (services.input().getMouseButtonDown(Input::LeftClick))
			{
				state.boxCombination = CombinationType::Addition;
				previewUi.material = state.previewAddMaterial.id;
				state.isCreatingBox = true;
			}
			else if (services.input().getMouseButtonDown(Input::RightClick))
			{
				state.boxCombination = CombinationType::Subtraction;
				previewUi.material = state.previewSubMaterial.id;
				state.isCreatingBox = true;
			}

			if (state.isCreatingBox)
			{
				vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
				state.boxStart = screen;

				// Position initial outline box cursor at click
				previewUi.parameters[DefaultShapes::BoxLine::PosX] = screen.x;
				previewUi.parameters[DefaultShapes::BoxLine::PosY] = screen.y;
				previewUi.parameters[DefaultShapes::BoxLine::SizeX] = 2.0f;
				previewUi.parameters[DefaultShapes::BoxLine::SizeY] = 2.0f;
				previewUi.parameters[DefaultShapes::BoxLine::Thickness] = 2.0f;
				registry.setComponentDirty(previewUi);
			}
		}

		if (state.isCreatingBox)
		{
			Input::MouseButton activeBtn =
				(state.boxCombination == CombinationType::Addition) ? Input::LeftClick : Input::RightClick;

			if (services.input().getMouseButton(activeBtn))
			{
				vec2 currentScreen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 pos = (currentScreen + state.boxStart) / 2.0f;
				vec2 size = 0.5f * glm::abs(currentScreen - state.boxStart);

				previewUi.parameters[DefaultShapes::BoxLine::PosX] = pos.x;
				previewUi.parameters[DefaultShapes::BoxLine::PosY] = pos.y;
				previewUi.parameters[DefaultShapes::BoxLine::SizeX] = std::max(size.x, 2.0f);
				previewUi.parameters[DefaultShapes::BoxLine::SizeY] = std::max(size.y, 2.0f);
				previewUi.parameters[DefaultShapes::BoxLine::Thickness] = 2.0f;
				registry.setComponentDirty(previewUi);
			}

			if (services.input().getMouseButtonUp(activeBtn))
			{
				state.isCreatingBox = false;

				auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
				vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 worldStart = ECS::Camera::screenPositionToWorldPosition2D(cam, state.boxStart);
				vec2 worldEnd = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

				vec2 pos = (worldEnd + worldStart) / 2.0f;
				vec2 size = 0.5f * glm::abs(worldEnd - worldStart);

				// If clicked without significant drag, spawn a handy default platform
				if (size.x < 0.4f && size.y < 0.4f)
				{
					size = vec2(2.5f, 0.35f);
				}
				else
				{
					size = glm::max(size, vec2(0.5f, 0.2f));
				}

				services.shapes().addShape({.shapeId = DefaultShapes::Box,
											.variables = {{DefaultShapes::Box::PosX, pos.x},
														  {DefaultShapes::Box::PosY, pos.y},
														  {DefaultShapes::Box::SizeX, size.x},
														  {DefaultShapes::Box::SizeY, size.y}},
											.material = services.materials2D().getHandle("platform"),
											.combination = state.boxCombination});

				// Hide preview box offscreen
				previewUi.parameters[DefaultShapes::BoxLine::PosX] = -1000.0f;
				previewUi.parameters[DefaultShapes::BoxLine::PosY] = -1000.0f;
				previewUi.parameters[DefaultShapes::BoxLine::SizeX] = 0.0f;
				previewUi.parameters[DefaultShapes::BoxLine::SizeY] = 0.0f;
				previewUi.parameters[DefaultShapes::BoxLine::Thickness] = 0.0f;
				registry.setComponentDirty(previewUi);
			}
		}
	}

	void walkingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		if (!registry.isEntityValid(state.player) || !registry.hasComponent<RigidBody2D>(state.player))
		{
			return;
		}

		float delta = services.time().deltaTime();
		if (delta <= 0.0f)
		{
			return;
		}

		auto& transform = registry.getComponent<Transform>(state.player);
		auto& rb = registry.getComponent<RigidBody2D>(state.player);
		auto& dot = registry.getComponent<Dot>(state.player);

		// Respawn if fallen into the void
		if (transform.position.y < -20.0f)
		{
			transform.position = vec3(0.0f, 4.0f, 0.0f);
			rb.velocity = vec2(0.0f, 0.0f);
			registry.setComponentDirty(transform);
			registry.setComponentDirty(rb);
			return;
		}

		// 1. Horizontal movement input: WASD (A/D), arrow keys, gamepad left stick, gamepad D-pad
		float inputX = 0.0f;
		if (services.input().getKey(Input::KeyCode::A) || services.input().getKey(Input::KeyCode::Left))
		{
			inputX -= 1.0f;
		}
		if (services.input().getKey(Input::KeyCode::D) || services.input().getKey(Input::KeyCode::Right))
		{
			inputX += 1.0f;
		}

		float gamepadX = services.input().getGamepadAxis(Input::GamepadAxis::LeftX);
		if (std::abs(gamepadX) > 0.15f)
		{
			inputX = gamepadX;
		}
		if (services.input().getGamepadButton(Input::GamepadButton::DpadLeft))
		{
			inputX = -1.0f;
		}
		if (services.input().getGamepadButton(Input::GamepadButton::DpadRight))
		{
			inputX = 1.0f;
		}

		inputX = std::clamp(inputX, -1.0f, 1.0f);

		// Apply wall jump lockout: prevent immediately canceling outward jump velocity
		if (state.wallJumpLockoutTimer > 0.0f)
		{
			state.wallJumpLockoutTimer -= delta;
			float awayDir = state.lastContactNormal.x;
			if ((awayDir > 0.0f && inputX < 0.0f) || (awayDir < 0.0f && inputX > 0.0f))
			{
				inputX = 0.0f;
			}
		}

		// 2. Jump input: WASD (W), Space, Up arrow, gamepad South (A), gamepad D-pad Up
		bool jumpHeld = services.input().getKey(Input::KeyCode::W) || services.input().getKey(Input::KeyCode::Space) ||
						services.input().getKey(Input::KeyCode::Up) ||
						services.input().getGamepadButton(Input::GamepadButton::South) ||
						services.input().getGamepadButton(Input::GamepadButton::DpadUp);

		bool jumpPressed = jumpHeld && !state.wasJumpHeld;
		bool jumpReleased = !jumpHeld && state.wasJumpHeld;
		state.wasJumpHeld = jumpHeld;

		// 3. Sample SDF distance and unit normal at player position
		vec2 playerPos = vec2(transform.position.x, transform.position.y);
		constexpr float RADIUS = 0.5f;
		constexpr float CONTACT_TOLERANCE = 0.16f;

		// Distance query from SDF (excluding dynamic rigidbodies)
		float distance = services.physics().sampleDistance(playerPos, false);

		bool inContact = (distance <= (RADIUS + CONTACT_TOLERANCE));
		bool groundedThisFrame = false;
		bool wallThisFrame = false;

		if (inContact)
		{
			// Gradient query from SDF (corresponds to outward unit normal at the surface)
			glm::vec2 normal = services.physics().sampleGradient(playerPos, false);
			state.lastContactNormal = normal;

			// Surface with predominantly upward normal counts as ground/slope
			if (normal.y >= 0.45f)
			{
				groundedThisFrame = (rb.velocity.y <= 0.5f);
			}
			// Steep vertical surface counts as wall
			else if (std::abs(normal.x) >= 0.35f)
			{
				wallThisFrame = true;
			}
		}

		// 4. Update Coyote Time & Ground State
		if (groundedThisFrame)
		{
			state.coyoteTimer = state.coyoteTime;
			state.isGrounded = true;
			state.isOnWall = false;
		}
		else
		{
			state.coyoteTimer -= delta;
			state.isGrounded = false;
			state.isOnWall = wallThisFrame;
		}

		if (jumpPressed)
		{
			state.jumpBufferTimer = state.jumpBufferTime;
		}
		else
		{
			state.jumpBufferTimer -= delta;
		}

		// 5. Jump Execution (unified for ground, slopes, and walls)
		if (state.jumpBufferTimer > 0.0f && (state.coyoteTimer > 0.0f || state.isOnWall))
		{
			vec2 n = state.lastContactNormal;

			// Normal-directed push + constant upward lift
			vec2 normalPush = n * state.jumpNormalPush;
			vec2 upwardLift = vec2(0.0f, state.jumpUpwardLift);

			vec2 jumpVel;
			if (state.isOnWall)
			{
				// Wall jump: kick away from the wall along the surface normal
				jumpVel.x = normalPush.x;
			}
			else
			{
				// Ground/slope jump: preserve running momentum (or input intent if jumping from rest)
				float horizontalMomentum =
					(std::abs(rb.velocity.x) > 0.5f) ? rb.velocity.x : (inputX * state.moveSpeed);
				jumpVel.x = horizontalMomentum + normalPush.x;
			}
			jumpVel.y = normalPush.y + upwardLift.y;

			// Clamp vertical and horizontal jump velocities
			jumpVel.y = std::clamp(jumpVel.y, state.minJumpVelocityY, state.maxJumpVelocityY);
			jumpVel.x = std::clamp(jumpVel.x, -state.maxJumpVelocityX, state.maxJumpVelocityX);

			float speed = glm::length(jumpVel);
			if (speed > state.maxJumpSpeed)
			{
				jumpVel = (jumpVel / speed) * state.maxJumpSpeed;
			}

			rb.velocity = jumpVel;

			// Reset timers
			state.jumpBufferTimer = 0.0f;
			state.coyoteTimer = 0.0f;
			state.isGrounded = false;
			state.isOnWall = false;

			// If leaping off a steep wall, activate brief directional lockout
			if (std::abs(n.x) > 0.4f && n.y < 0.6f)
			{
				state.wallJumpLockoutTimer = state.wallJumpLockout;
			}
		}
		// Variable jump height: cut vertical velocity if jump is released early while rising
		else if (jumpReleased && rb.velocity.y > 0.0f)
		{
			rb.velocity.y *= state.jumpCutMultiplier;
		}

		// 6. Horizontal movement (applied directly to kinematic velocity)
		float targetVx = inputX * state.moveSpeed;
		float accel = 0.0f;
		if (state.isGrounded)
		{
			accel = (std::abs(targetVx) > 0.01f) ? state.groundAccel : state.groundDecel;
		}
		else
		{
			accel = (std::abs(targetVx) > 0.01f) ? state.airAccel : state.airDecel;
		}

		float maxDeltaVx = accel * delta;
		float deltaVx = std::clamp(targetVx - rb.velocity.x, -maxDeltaVx, maxDeltaVx);
		rb.velocity.x += deltaVx;

		// 7. Vertical movement (kinematic gravity & wall slide)
		if (state.isGrounded)
		{
			// When grounded on a surface, zero out downward vertical velocity so the
			// kinematic body rests cleanly without pushing into the floor.
			if (rb.velocity.y < 0.0f)
			{
				rb.velocity.y = 0.0f;
			}
		}
		else
		{
			// Kinematic bodies do not receive world physics gravity, so apply custom platformer gravity
			rb.velocity.y -= state.gravity * delta;

			// Wall slide: limit downward sliding speed
			if (state.isOnWall && rb.velocity.y < -state.wallSlideSpeed)
			{
				rb.velocity.y = -state.wallSlideSpeed;
			}

			// Terminal fall velocity clamp
			if (rb.velocity.y < state.terminalFallVelocity)
			{
				rb.velocity.y = state.terminalFallVelocity;
			}
		}

		if (rb.velocity.y > state.maxUpwardVelocity)
		{
			rb.velocity.y = state.maxUpwardVelocity;
		}

		// Synchronize kinematic velocity to the physics thread
		registry.setComponentDirty(rb);

		// 8. Update visual feedback material: Yellow on floor, Orange on wall, Magenta in air
		if (state.isGrounded)
		{
			dot.materialId = state.floorMaterial.id;
		}
		else if (state.isOnWall)
		{
			dot.materialId = state.wallMaterial.id;
		}
		else
		{
			dot.materialId = state.airMaterial.id;
		}
	}

	void cameraFollowSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		if (!registry.isEntityValid(state.player) || !registry.hasComponent<Transform>(state.player))
		{
			return;
		}

		float delta = services.time().deltaTime();
		auto& playerTransform = registry.getComponent<Transform>(state.player);
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());

		constexpr float FOLLOW_SPEED = 6.0f;
		cameraTransform.position.x =
			glm::mix(cameraTransform.position.x, playerTransform.position.x, FOLLOW_SPEED * delta);
		cameraTransform.position.y =
			glm::mix(cameraTransform.position.y, playerTransform.position.y + 2.0f, FOLLOW_SPEED * delta);
		cameraTransform.position.z = 25.0f;
	}
} // namespace WalkSceneNamespace