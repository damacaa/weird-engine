#include "ThrustScene.h"

#include "weird-renderer/core/Display.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/constants.hpp>

using namespace WeirdEngine;

namespace ThrustSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "ThrustScene State is missing: stateInitSystem must run first");
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
		background.primaryColor = vec4(0.02f, 0.04f, 0.09f, 1.0f);
		background.secondaryColor = vec4(0.07f, 0.11f, 0.18f, 1.0f);
		background.scale = 0.15f;

		// Gravity in physics simulation for exhaust particles bouncing off cave terrain
		services.physics().setGravity(-3.0f);
		services.physics().setDamping(0.01f);
		services.physics().setFriction(0.2f);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 6.0f, 30.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}

	void loadShipSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Random materials from palette (no createMaterial calls)
		uint16_t matCount = services.materials2D().getMaterialCount();
		if (matCount == 0)
		{
			matCount = 16;
		}

		auto getRandomMat = [&](uint16_t fallbackMin = 0) -> Material2DHandle
		{
			uint16_t idx = static_cast<uint16_t>(services.random().range<int>(5, matCount - 1));
			return services.materials2D().get(idx);
		};

		state.shipMaterial = getRandomMat();
		state.exhaustMaterial = getRandomMat();
		state.fuelMaterial = getRandomMat();
		state.fuelBgMaterial = getRandomMat();

		// 1. Ship visual shape (SDF TriangleRotated, collisions handled via kinematic rigidbodies)
		state.shipShapeEntity =
			services.shapes().addShape({.shapeId = DefaultShapes::TriangleRotated,
										.variables = {{DefaultShapes::TriangleRotated::PosX, state.shipPos.x},
													  {DefaultShapes::TriangleRotated::PosY, state.shipPos.y},
													  {DefaultShapes::TriangleRotated::Width, state.shipWidth},
													  {DefaultShapes::TriangleRotated::Height, state.shipHeight},
													  {DefaultShapes::TriangleRotated::Angle, state.shipAngle}},
										.material = state.shipMaterial,
										.hasCollision = false});

		// 2. Kinematic rigidbodies distributed across the ship body
		for (size_t i = 0; i < NUM_KINEMATIC_BODIES; ++i)
		{
			Entity e = registry.createEntity();
			auto& t = registry.addComponent<Transform>(e);
			t.position = vec3(state.shipPos + state.shipKinematicOffsets[i], 0.0f);
			registry.setEntityDirty<Transform>(e, true);

			auto& rb = registry.addComponent<RigidBody2D>(e);
			rb.type = BodyType::Kinematic;
			rb.mass = 1.0f;
			rb.enableCollision = false;		// Prevent dot-to-dot collisions affecting the ship
			rb.enableShapeCollision = true; // Enable collisions with map terrain shapes
			registry.setEntityDirty<RigidBody2D>(e, true);

			state.shipKinematicBodies[i] = e;
		}
	}

	void setupUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float screenH = static_cast<float>(Display::height);
		float startX = 60.0f;
		float startY = screenH - 45.0f;
		float barWidth = 200.0f;

		// Fuel bar track (background line)
		state.fuelBgLineEntity =
			services.shapes().addUIShape({.shapeId = DefaultShapes::Line,
										  .variables = {{DefaultShapes::Line::StartX, startX},
														{DefaultShapes::Line::StartY, startY},
														{DefaultShapes::Line::EndX, startX + barWidth},
														{DefaultShapes::Line::EndY, startY},
														{DefaultShapes::Line::Width, 4.5f}},
										  .material = state.fuelBgMaterial});

		// Active fuel line
		state.fuelLineEntity =
			services.shapes().addUIShape({.shapeId = DefaultShapes::Line,
										  .variables = {{DefaultShapes::Line::StartX, startX},
														{DefaultShapes::Line::StartY, startY},
														{DefaultShapes::Line::EndX, startX + barWidth},
														{DefaultShapes::Line::EndY, startY},
														{DefaultShapes::Line::Width, 3.0f}},
										  .material = state.fuelMaterial});

		// Fuel text label
		state.fuelLabelEntity = registry.createEntity();
		auto& t = registry.addComponent<Transform>(state.fuelLabelEntity);
		t.position = vec3(startX, startY + 16.0f, 0.0f);
		auto& tr = registry.addComponent<UITextRenderer>(state.fuelLabelEntity);
		tr.text = "FUEL";
		tr.material = state.shipMaterial.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
	}

	void onShapeCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		State& state = getState(registry);

		// Identify whether this collision event hit any of the ship's kinematic probes
		bool isShipBody = false;
		for (Entity b : state.shipKinematicBodies)
		{
			if (event.entity == b)
			{
				isShipBody = true;
				break;
			}
		}
		if (!isShipBody)
		{
			return;
		}

		// Ensure collisions with the ship's own shape are ignored
		if (registry.isEntityValid(state.shipShapeEntity) && registry.hasComponent<Shape>(state.shipShapeEntity))
		{
			const auto& shipShape = registry.getComponent<Shape>(state.shipShapeEntity);
			if (event.raw.shape == shipShape.simulationId)
			{
				return;
			}
		}

		if (event.raw.state == CollisionState::START || event.raw.state == CollisionState::CONTINUE)
		{
			vec2 normal = event.raw.normal;
			if (glm::length2(normal) > 0.0001f)
			{
				normal = glm::normalize(normal);
				float pen = std::max(event.raw.penetration, 0.12f);

				// Push back ship position away from colliding map geometry
				state.shipPos += normal * pen;

				// Elastic bounce reflection
				float vn = glm::dot(state.shipVelocity, normal);
				if (vn < 0.0f)
				{
					constexpr float RESTITUTION = 0.55f;
					state.shipVelocity -= (1.0f + RESTITUTION) * vn * normal;
				}
				state.shipVelocity += normal * 2.5f;
			}
		}
	}

	void shipPhysicsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();
		if (delta <= 0.0f)
		{
			return;
		}

		// 1. Rotation input (A/D, Left/Right arrow, gamepad LeftStick / Dpad)
		float rotateInput = 0.0f;
		if (services.input().getKey(Input::KeyCode::A) || services.input().getKey(Input::KeyCode::Left))
		{
			rotateInput -= 1.0f;
		}
		if (services.input().getKey(Input::KeyCode::D) || services.input().getKey(Input::KeyCode::Right))
		{
			rotateInput += 1.0f;
		}
		float stickX = services.input().getGamepadAxis(Input::GamepadAxis::LeftX);
		if (std::abs(stickX) > 0.2f)
		{
			rotateInput = stickX;
		}
		if (services.input().getGamepadButton(Input::GamepadButton::DpadLeft))
		{
			rotateInput = -1.0f;
		}
		if (services.input().getGamepadButton(Input::GamepadButton::DpadRight))
		{
			rotateInput = 1.0f;
		}

		state.shipAngle += rotateInput * state.rotationSpeed * delta;
		while (state.shipAngle > glm::pi<float>())
		{
			state.shipAngle -= glm::two_pi<float>();
		}
		while (state.shipAngle < -glm::pi<float>())
		{
			state.shipAngle += glm::two_pi<float>();
		}

		// 2. Thrust input (W, Up, Space, gamepad South / RightTrigger)
		bool thrustHeld = services.input().getKey(Input::KeyCode::W) || services.input().getKey(Input::KeyCode::Up) ||
						  services.input().getKey(Input::KeyCode::Space) ||
						  services.input().getGamepadButton(Input::GamepadButton::South) ||
						  services.input().getGamepadAxis(Input::GamepadAxis::RightTrigger) > 0.3f;

		state.isThrusting = thrustHeld && (state.fuel > 0.0f);

		// Forward direction: angle 0 points up (+Y)
		vec2 forward = vec2(std::sin(state.shipAngle), std::cos(state.shipAngle));
		vec2 right = vec2(std::cos(state.shipAngle), -std::sin(state.shipAngle));

		if (state.isThrusting)
		{
			state.shipVelocity += forward * (state.thrustPower * delta);
			state.fuel = std::max(0.0f, state.fuel - state.fuelBurnRate * delta);

			uint16_t matCount = services.materials2D().getMaterialCount();
			if (matCount == 0)
			{
				matCount = 16;
			}

			vec2 nozzlePos = state.shipPos - forward * (state.shipHeight * 0.33f);
			constexpr int PARTICLES_PER_FRAME = 3;

			for (int i = 0; i < PARTICLES_PER_FRAME; ++i)
			{
				auto& slot = state.exhaustParticles[state.exhaustHead];
				if (registry.isEntityValid(slot.entity))
				{
					registry.destroyEntity(slot.entity);
					slot.entity = INVALID_ENTITY;
				}

				Entity dotEntity = registry.createEntity();
				auto& t = registry.addComponent<Transform>(dotEntity);

				float spreadX = (static_cast<float>(i) - 1.0f) * 0.6f + services.random().range(-0.1f, 0.1f);
				float spreadY = services.random().range(-0.25f, 0.25f);
				t.position = vec3(nozzlePos + right * spreadX - forward * spreadY, 0.0f);
				registry.setEntityDirty<Transform>(dotEntity, true);

				auto& dot = registry.addComponent<Dot>(dotEntity);
				dot.materialId =
					services.materials2D().get(static_cast<uint16_t>(services.random().range<int>(0, matCount - 1))).id;

				auto& rb = registry.addComponent<RigidBody2D>(dotEntity);
				rb.type = BodyType::Dynamic;
				rb.mass = 0.05f;

				float speed = services.random().range(14.0f, 30.0f);
				// Wide angular and directional spread
				float angleNoise = services.random().range(-0.25f, 0.25f);
				vec2 particleDir = -forward + right * angleNoise;
				vec2 particleVel = state.shipVelocity + glm::normalize(particleDir) * speed;
				rb.velocity = particleVel;
				rb.enableCollision = true;
				rb.enableShapeCollision = true; // Collide with map shapes
				registry.setEntityDirty<RigidBody2D>(dotEntity, true);

				slot.entity = dotEntity;
				// Make dots last longer (3.5 to 6.0 seconds)
				slot.lifetime = services.random().range(3.5f, 6.0f);

				state.exhaustHead = (state.exhaustHead + 1) % MAX_EXHAUST_PARTICLES;
			}
		}

		// 3. Gravity
		state.shipVelocity.y -= state.gravity * delta;

		// 4. Linear drag
		state.shipVelocity *= std::clamp(1.0f - state.linearDrag * delta, 0.0f, 1.0f);

		// 5. Integrate position
		state.shipPos += state.shipVelocity * delta;

		// 6. Proximity SDF probe guard: push back if any kinematic point intersects terrain
		constexpr float PROBE_RADIUS = 0.50f;
		for (size_t i = 0; i < NUM_KINEMATIC_BODIES; ++i)
		{
			vec2 local = state.shipKinematicOffsets[i];
			float ca = std::cos(state.shipAngle);
			float sa = std::sin(state.shipAngle);
			vec2 rot = vec2(ca * local.x - sa * local.y, sa * local.x + ca * local.y);
			vec2 worldProbe = state.shipPos + rot;

			float dist = services.physics().sampleDistance(worldProbe, false);
			if (dist < PROBE_RADIUS)
			{
				vec2 normal = services.physics().sampleGradient(worldProbe, false);
				if (glm::length2(normal) > 0.0001f)
				{
					normal = glm::normalize(normal);
					float penetration = PROBE_RADIUS - dist;
					state.shipPos += normal * penetration;

					float vn = glm::dot(state.shipVelocity, normal);
					if (vn < 0.0f)
					{
						state.shipVelocity -= 1.55f * vn * normal;
					}
				}
			}
		}

		// 7. Respawn / Refuel on 'R' key, gamepad North (Y button), or void fall
		if (services.input().getKeyDown(Input::KeyCode::R) ||
			services.input().getGamepadButtonDown(Input::GamepadButton::North) || state.shipPos.y < -30.0f)
		{
			state.shipPos = vec2(0.0f, 6.0f);
			state.shipVelocity = vec2(0.0f, 0.0f);
			state.shipAngle = 0.0f;
			state.fuel = state.maxFuel;
		}

		// Refuel slowly when resting stably near the start area
		if (std::abs(state.shipPos.x) < 5.0f && state.shipPos.y < 7.0f && glm::length(state.shipVelocity) < 1.0f)
		{
			state.fuel = std::min(state.maxFuel, state.fuel + 20.0f * delta);
		}

		// 8. Synchronize kinematic rigidbodies transforms and velocities
		for (size_t i = 0; i < NUM_KINEMATIC_BODIES; ++i)
		{
			Entity bodyEnt = state.shipKinematicBodies[i];
			if (registry.isEntityValid(bodyEnt))
			{
				vec2 local = state.shipKinematicOffsets[i];
				float ca = std::cos(state.shipAngle);
				float sa = std::sin(state.shipAngle);
				vec2 rot = vec2(ca * local.x - sa * local.y, sa * local.x + ca * local.y);
				vec2 worldPos = state.shipPos + rot;

				auto& t = registry.getComponent<Transform>(bodyEnt);
				t.position = vec3(worldPos, 0.0f);
				registry.setEntityDirty<Transform>(bodyEnt, true);

				auto& rb = registry.getComponent<RigidBody2D>(bodyEnt);
				rb.velocity = state.shipVelocity;
				registry.setEntityDirty<RigidBody2D>(bodyEnt, true);
			}
		}

		// 9. Synchronize ship visual shape
		if (registry.isEntityValid(state.shipShapeEntity) && registry.hasComponent<Shape>(state.shipShapeEntity))
		{
			auto& shape = registry.getComponent<Shape>(state.shipShapeEntity);
			shape.parameters[DefaultShapes::TriangleRotated::PosX] = state.shipPos.x;
			shape.parameters[DefaultShapes::TriangleRotated::PosY] = state.shipPos.y;
			shape.parameters[DefaultShapes::TriangleRotated::Width] = state.shipWidth;
			shape.parameters[DefaultShapes::TriangleRotated::Height] = state.shipHeight;
			shape.parameters[DefaultShapes::TriangleRotated::Angle] = state.shipAngle;
		}
	}

	void exhaustParticleSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		for (auto& slot : state.exhaustParticles)
		{
			if (registry.isEntityValid(slot.entity))
			{
				slot.lifetime -= delta;
				if (slot.lifetime <= 0.0f)
				{
					registry.destroyEntity(slot.entity);
					slot.entity = INVALID_ENTITY;
				}
			}
		}
	}

	void uiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float screenH = static_cast<float>(Display::height);
		float startX = 60.0f;
		float startY = screenH - 45.0f;
		float totalWidth = 200.0f;
		float fuelRatio = std::clamp(state.fuel / state.maxFuel, 0.0f, 1.0f);

		if (registry.isEntityValid(state.fuelBgLineEntity) && registry.hasComponent<UIShape>(state.fuelBgLineEntity))
		{
			auto& bgUi = registry.getComponent<UIShape>(state.fuelBgLineEntity);
			bgUi.parameters[DefaultShapes::Line::StartX] = startX;
			bgUi.parameters[DefaultShapes::Line::StartY] = startY;
			bgUi.parameters[DefaultShapes::Line::EndX] = startX + totalWidth;
			bgUi.parameters[DefaultShapes::Line::EndY] = startY;
			bgUi.parameters[DefaultShapes::Line::Width] = 4.5f;
		}

		if (registry.isEntityValid(state.fuelLineEntity) && registry.hasComponent<UIShape>(state.fuelLineEntity))
		{
			auto& fuelUi = registry.getComponent<UIShape>(state.fuelLineEntity);
			fuelUi.parameters[DefaultShapes::Line::StartX] = startX;
			fuelUi.parameters[DefaultShapes::Line::StartY] = startY;
			fuelUi.parameters[DefaultShapes::Line::EndX] = startX + totalWidth * fuelRatio;
			fuelUi.parameters[DefaultShapes::Line::EndY] = startY;
			fuelUi.parameters[DefaultShapes::Line::Width] = fuelRatio > 0.001f ? 3.0f : 0.0f;
		}

		if (registry.isEntityValid(state.fuelLabelEntity) && registry.hasComponent<Transform>(state.fuelLabelEntity))
		{
			auto& t = registry.getComponent<Transform>(state.fuelLabelEntity);
			t.position = vec3(startX, startY + 16.0f, 0.0f);
		}
	}

	void cameraFollowSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		constexpr float FOLLOW_SPEED = 5.0f;
		cameraTransform.position.x = glm::mix(cameraTransform.position.x, state.shipPos.x, FOLLOW_SPEED * delta);
		cameraTransform.position.y = glm::mix(cameraTransform.position.y, state.shipPos.y + 1.8f, FOLLOW_SPEED * delta);
		cameraTransform.position.z = 30.0f;
	}
} // namespace ThrustSceneNamespace
