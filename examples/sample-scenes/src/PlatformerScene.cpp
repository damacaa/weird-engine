#include "PlatformerScene.h"

#include <algorithm>
#include <cmath>

using namespace WeirdEngine;

namespace PlatformerSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "PlatformerScene State is missing: stateInitSystem must run first");
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

		services.physics().setGravity(-25.0f);
		services.physics().setDamping(0.0f);
		services.physics().setFriction(0.0f);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 4.0f, 25.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}

	void loadCharacterSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		const auto& floorMat = services.materials2D().getHandle("player_floor");
		state.floorMaterial = floorMat;

		const auto& wallMat = services.materials2D().getHandle("player_wall");
		state.wallMaterial = wallMat;

		const auto& airMat = services.materials2D().getHandle("player_air");
		state.airMaterial = airMat;

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

		const auto& addMat = services.materials2D().getHandle("ui_box_preview_add");
		state.previewAddMaterial = addMat;

		const auto& subMat = services.materials2D().getHandle("ui_box_preview_sub");
		state.previewSubMaterial = subMat;

		// UI preview box line cursor for platform creation (initially offscreen)
		state.previewBox = services.shapes().addUIShape({.shapeId = DefaultShapes::BoxLine,
														 .variables = {{DefaultShapes::BoxLine::PosX, -1000.0f},
																	   {DefaultShapes::BoxLine::PosY, -1000.0f},
																	   {DefaultShapes::BoxLine::SizeX, 0.0f},
																	   {DefaultShapes::BoxLine::SizeY, 0.0f},
																	   {DefaultShapes::BoxLine::Thickness, 2.0f}},
														 .material = state.previewAddMaterial});
	}

	void onShapeCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		State& state = getState(registry);
		if (event.entity == state.player && event.raw.state == CollisionState::START)
		{
			float normalSpeed = std::abs(glm::dot(event.raw.normal, event.raw.velocity));
			if (normalSpeed > 0.25f)
			{
				state.playedImpactThisFrame = true;
			}
		}
	}

	void platformBuilderSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (!registry.isEntityValid(state.previewBox) || !registry.hasComponent<UIShape>(state.previewBox))
		{
			return;
		}

		auto& previewUi = registry.getComponent<UIShape>(state.previewBox);
		auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());

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
				state.boxStart = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

				// Position initial outline box cursor at click
				previewUi.parameters[DefaultShapes::BoxLine::PosX] = screen.x;
				previewUi.parameters[DefaultShapes::BoxLine::PosY] = screen.y;
				previewUi.parameters[DefaultShapes::BoxLine::SizeX] = 2.0f;
				previewUi.parameters[DefaultShapes::BoxLine::SizeY] = 2.0f;
				previewUi.parameters[DefaultShapes::BoxLine::Thickness] = 2.0f;
				registry.setComponentDirty(previewUi);
				services.render().forceShaderRefreshUI();
			}
		}

		if (state.isCreatingBox)
		{
			Input::MouseButton activeBtn =
				(state.boxCombination == CombinationType::Addition) ? Input::LeftClick : Input::RightClick;

			if (services.input().getMouseButton(activeBtn))
			{
				vec2 currentScreen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 startScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, state.boxStart);
				vec2 pos = (currentScreen + startScreen) / 2.0f;
				vec2 size = 0.5f * glm::abs(currentScreen - startScreen);

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

				vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 worldEnd = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

				vec2 pos = (worldEnd + state.boxStart) / 2.0f;
				vec2 size = 0.5f * glm::abs(worldEnd - state.boxStart);

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
		// Note: Simulation2D::map() subtracts 0.05f from raw SDF distances, so the physics collision
		// boundary rests at raw sampleDistance == 0.55f (0.50f radius + 0.05f physics margin).
		constexpr float PHYSICS_SURFACE_DIST = 0.55f;
		constexpr float LANDING_TOLERANCE = 0.03f;
		constexpr float GROUND_STICK_TOLERANCE = 0.11f;
		constexpr float WALL_TOLERANCE = 0.04f;

		if (state.jumpGraceTimer > 0.0f)
		{
			state.jumpGraceTimer -= delta;
		}

		// Distance query from SDF (excluding dynamic rigidbodies)
		float distance = services.physics().sampleDistance(playerPos, false);

		// Require actual physics surface contact to land from the air or grab a wall,
		// while using stick tolerance once grounded so walking downhill stays attached
		float groundTolerance = state.isGrounded ? GROUND_STICK_TOLERANCE : LANDING_TOLERANCE;
		bool groundedThisFrame = false;
		bool slidingThisFrame = false;
		bool wallThisFrame = false;

		if (state.jumpGraceTimer <= 0.0f && distance <= (PHYSICS_SURFACE_DIST + GROUND_STICK_TOLERANCE))
		{
			// Gradient query from SDF (corresponds to outward unit normal at the surface)
			glm::vec2 normal = services.physics().sampleGradient(playerPos, false);

			// Surface with predominantly upward normal counts as ground/slope
			if (normal.y >= state.minGroundNormalY)
			{
				if (distance <= (PHYSICS_SURFACE_DIST + groundTolerance))
				{
					state.lastContactNormal = normal;
					// Check normal-relative velocity so walking uphill (positive velocity.y) stays grounded
					float normalVel = glm::dot(rb.velocity, normal);
					groundedThisFrame = (normalVel <= 0.5f);
					if (groundedThisFrame && normal.y < state.maxWalkableNormalY)
					{
						slidingThisFrame = true;
					}
				}
			}
			// Steep vertical surface counts as wall (tight tolerance so player must touch the wall)
			else if (std::abs(normal.x) >= 0.35f && distance <= (PHYSICS_SURFACE_DIST + WALL_TOLERANCE))
			{
				state.lastContactNormal = normal;
				wallThisFrame = true;
			}
		}

		// 4. Ground state & landing impact audio
		bool justLanded = !state.isGrounded && groundedThisFrame;
		if (groundedThisFrame)
		{
			// Consistent landing audio: play if landing with downward velocity and not already triggered by physics
			if (justLanded)
			{
				vec2 impactVel = (glm::length2(state.lastAirborneVelocity) > glm::length2(rb.velocity))
									 ? state.lastAirborneVelocity
									 : rb.velocity;
				float impactSpeed = -glm::dot(impactVel, state.lastContactNormal);
				if (impactSpeed > 1.2f && !state.playedImpactThisFrame)
				{
					float penetrationFactor =
						std::sqrt((std::min)(2.0f * std::max(0.0f, PHYSICS_SURFACE_DIST - distance), 1.0f));
					float intensity = std::clamp(impactSpeed * 0.12f + penetrationFactor * 0.3f, 0.05f, 1.0f);
					services.audio().playCollisionSound(transform.position, intensity,
														WeirdAudio::SimpleAudioRequest::ImpactType::Shape);
				}
			}

			state.coyoteTimer = state.coyoteTime;
			state.isGrounded = true;
			state.isSliding = slidingThisFrame;
			state.isOnWall = false;
		}
		else
		{
			state.coyoteTimer -= delta;
			state.isGrounded = false;
			state.isSliding = false;
			state.isOnWall = wallThisFrame;
		}

		state.playedImpactThisFrame = false;

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

			// Reset timers and activate jump grace period so async physics readback cannot re-ground the jump
			state.jumpBufferTimer = 0.0f;
			state.coyoteTimer = 0.0f;
			state.jumpGraceTimer = 0.08f;
			state.isGrounded = false;
			state.isSliding = false;
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

		// 6. Surface movement (ground & slopes) or airborne control
		if (state.isGrounded)
		{
			vec2 normal = state.lastContactNormal;
			// Unit surface tangent pointing rightward: (normal.y, -normal.x)
			vec2 tangent = vec2(normal.y, -normal.x);
			if (tangent.x < 0.0f)
			{
				tangent = -tangent;
			}

			if (state.isSliding)
			{
				// --- STEEP SLOPE SLIDING ---
				// Gravitational acceleration projected along surface tangent
				// normal.x > 0 means slope falls to the right; normal.x < 0 means slope falls to the left
				float downhillAccel = state.gravity * normal.x * state.slopeSlideMultiplier;

				// Allow player to steer slightly or accelerate downhill, but heavily penalize running uphill
				float steer = inputX;
				if ((normal.x > 0.0f && steer < 0.0f) || (normal.x < 0.0f && steer > 0.0f))
				{
					steer *= 0.15f; // Hard to run up a steep slope
				}

				float currentTangentSpeed = glm::dot(rb.velocity, tangent);
				currentTangentSpeed += (downhillAccel + steer * state.groundAccel * 0.35f) * delta;
				currentTangentSpeed = std::clamp(currentTangentSpeed, -state.maxSlideSpeed, state.maxSlideSpeed);

				rb.velocity = tangent * currentTangentSpeed;
			}
			else
			{
				// --- WALKABLE GROUND & SLOPES ---
				float targetSpeed = inputX * state.moveSpeed;
				float currentSpeed = glm::dot(rb.velocity, tangent);

				float accel = 0.0f;
				if (std::abs(targetSpeed) > 0.01f)
				{
					accel = state.groundAccel;

					// Add subtle downhill boost / uphill resistance for a weighted, physical feel
					if ((inputX > 0.0f && normal.x > 0.0f) || (inputX < 0.0f && normal.x < 0.0f))
					{
						targetSpeed += std::copysign(1.5f, inputX);
					}
				}
				else
				{
					accel = state.groundDecel;
				}

				float maxDeltaSpeed = accel * delta;
				float deltaSpeed = std::clamp(targetSpeed - currentSpeed, -maxDeltaSpeed, maxDeltaSpeed);
				float newSpeed = currentSpeed + deltaSpeed;

				rb.velocity = tangent * newSpeed;
			}

			// Eliminate any floor gap without overriding physics transform:
			// gently pull into the surface until flush at PHYSICS_SURFACE_DIST, where Simulation2D PBD depenetration
			// holds it
			float gap = distance - PHYSICS_SURFACE_DIST;
			if (gap > 0.002f)
			{
				float pullSpeed = std::min(gap * 25.0f + 0.5f, 6.0f);
				rb.velocity -= normal * pullSpeed;
			}

			state.lastAirborneVelocity = vec2(0.0f, 0.0f);
		}
		else if (state.jumpGraceTimer <= 0.0f)
		{
			// --- AIRBORNE MOVEMENT ---
			// Horizontal air steering
			float targetVx = inputX * state.moveSpeed;
			float accel = (std::abs(targetVx) > 0.01f) ? state.airAccel : state.airDecel;
			float maxDeltaVx = accel * delta;
			float deltaVx = std::clamp(targetVx - rb.velocity.x, -maxDeltaVx, maxDeltaVx);
			rb.velocity.x += deltaVx;

			// Asymmetric gravity: fall faster than rise for crisp, weighty platformer arcs
			float currentGravity = state.gravity;
			if (rb.velocity.y < 0.0f)
			{
				currentGravity *= 1.35f;
			}
			rb.velocity.y -= currentGravity * delta;

			// Wall slide: limit downward sliding speed when hugging a wall
			if (state.isOnWall && rb.velocity.y < -state.wallSlideSpeed)
			{
				rb.velocity.y = -state.wallSlideSpeed;
			}

			// Terminal fall velocity clamp
			if (rb.velocity.y < state.terminalFallVelocity)
			{
				rb.velocity.y = state.terminalFallVelocity;
			}

			state.lastAirborneVelocity = rb.velocity;
		}
		else
		{
			state.lastAirborneVelocity = rb.velocity;
		}

		if (rb.velocity.y > state.maxUpwardVelocity)
		{
			rb.velocity.y = state.maxUpwardVelocity;
		}

		// Synchronize kinematic velocity to the physics thread
		registry.setComponentDirty(rb);

		// 8. Update visual feedback material: Yellow on floor, Orange on wall/slope slide, Magenta in air
		if (state.isGrounded)
		{
			dot.materialId = state.isSliding ? state.wallMaterial.id : state.floorMaterial.id;
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
} // namespace PlatformerSceneNamespace