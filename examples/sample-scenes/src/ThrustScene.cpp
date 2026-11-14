#include "ThrustScene.h"

#include "weird-renderer/core/Display.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <glm/gtc/constants.hpp>
#include <span>

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
		services.physics().setDamping(0.003f);
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

		state.fuelLabelMaterial = getRandomMat();
		state.fuelMaterial = getRandomMat();
		state.fuelBgMaterial = getRandomMat();

		// Load spaceship cluster asset and drive its dots kinematically
		auto& cluster = services.clusters().instantiate(services.resources().assetPath("spaceship.weirdcluster"),
														state.shipPos, 0.0f, "spaceship", {"ship"});

		vec2 centroid(0.0f);
		size_t dotCount = 0;
		for (Entity e : cluster.entities)
		{
			if (registry.hasComponent<Transform>(e) && registry.hasComponent<RigidBody2D>(e))
			{
				centroid += vec2(registry.getComponent<Transform>(e).position);
				++dotCount;
			}
		}
		if (dotCount > 0)
		{
			centroid /= static_cast<float>(dotCount);
		}
		else
		{
			centroid = state.shipPos;
		}

		float minLocalY = 0.0f;
		state.shipDotCount = 0;
		for (Entity e : cluster.entities)
		{
			if (!registry.hasComponent<Transform>(e) || !registry.hasComponent<RigidBody2D>(e))
				continue;

			if (state.shipDotCount >= MAX_SHIP_DOTS)
			{
				WEIRD_ASSERT(false, "spaceship.weirdcluster dot count exceeds MAX_SHIP_DOTS");
				break;
			}

			auto& t = registry.getComponent<Transform>(e);
			vec2 localOffset = vec2(t.position) - centroid;
			minLocalY = std::min(minLocalY, localOffset.y);

			t.position = vec3(state.shipPos + localOffset, 0.0f);
			registry.setEntityDirty<Transform>(e, true);

			auto& rb = registry.getComponent<RigidBody2D>(e);
			rb.type = BodyType::Kinematic;
			rb.mass = 1.0f;
			rb.enableCollision = false;		// Prevent cluster dot-to-dot collisions and nozzle snagging
			rb.enableShapeCollision = true; // Enable collisions with map terrain shapes
			registry.setEntityDirty<RigidBody2D>(e, true);

			state.shipDots[state.shipDotCount++] = {e, localOffset};
		}

		// Place exhaust emission point just behind the rear thruster pods
		state.thrusterLocalY = minLocalY - 0.36f;
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
		tr.material = state.fuelLabelMaterial.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
	}

	namespace
	{
		void resolveShipTerrainCollisions(State& state, ServiceProvider& services, const vec2& forward,
										  const vec2& right)
		{
			constexpr float PROBE_RADIUS = 0.50f;
			constexpr float BOUNCE_SPEED_THRESHOLD = 1.5f;
			constexpr float RESTITUTION = 0.45f;

			for (size_t i = 0; i < state.shipDotCount; ++i)
			{
				const vec2 local = state.shipDots[i].localOffset;
				const vec2 worldProbe = state.shipPos + right * local.x + forward * local.y;

				const float dist = services.physics().sampleDistance(worldProbe, false);
				if (dist >= PROBE_RADIUS)
					continue;

				vec2 normal = services.physics().sampleGradient(worldProbe, false);
				if (glm::length2(normal) <= 0.0001f)
					continue;

				normal = glm::normalize(normal);
				state.shipPos += normal * (PROBE_RADIUS - dist);

				const float vn = glm::dot(state.shipVelocity, normal);
				if (vn < 0.0f)
				{
					const float restitution = (-vn > BOUNCE_SPEED_THRESHOLD) ? RESTITUTION : 0.0f;
					state.shipVelocity -= (1.0f + restitution) * vn * normal;
				}
			}
		}

		struct ExhaustLane
		{
			float localX;
			float fanSlope;
		};

		constexpr ExhaustLane WAVE_A_LANES[] = {
			{-1.08f, -0.16f},
			{0.0f, 0.0f},
			{1.08f, 0.16f},
		};

		constexpr ExhaustLane WAVE_B_BASE_LANES[] = {
			{-0.54f, -0.08f},
			{0.54f, 0.08f},
		};

		constexpr ExhaustLane WAVE_B_HIGH_SPEED_LANES[] = {
			{-1.62f, -0.24f},
			{-0.54f, -0.08f},
			{0.54f, 0.08f},
			{1.62f, 0.24f},
		};

		constexpr float RECENT_SPAWN_TRACK_WINDOW = 0.25f;
		constexpr float PHYSICS_DAMPING_APPROX = 0.3f; // Matches 0.003f damping at 100 Hz

		void advanceRecentExhaustSpawns(State& state, float delta)
		{
			const float dampingScale = std::max(0.0f, 1.0f - PHYSICS_DAMPING_APPROX * delta);
			for (auto& recent : state.recentExhaustSpawns)
			{
				if (recent.age < RECENT_SPAWN_TRACK_WINDOW)
				{
					recent.pos += recent.vel * delta;
					recent.vel *= dampingScale;
					recent.age += delta;
				}
			}
		}

		bool overlapsRecentExhaust(const State& state, const vec2& candidatePos, float minDistSq)
		{
			for (const auto& recent : state.recentExhaustSpawns)
			{
				if (recent.age >= RECENT_SPAWN_TRACK_WINDOW)
					continue;

				const vec2 diff = candidatePos - recent.pos;
				if ((diff.x * diff.x + diff.y * diff.y) < minDistSq)
					return true;
			}
			return false;
		}

		void recordRecentExhaustSpawn(State& state, const vec2& pos, const vec2& vel, float age)
		{
			state.recentExhaustSpawns[state.recentExhaustHead] = {pos, vel, age};
			state.recentExhaustHead = (state.recentExhaustHead + 1) % MAX_RECENT_EXHAUST_SPAWNS;
		}

		void spawnExhaustDot(State& state, Registry& registry, ServiceProvider& services, const vec2& spawnPos,
							 const vec2& worldVel, uint16_t matCount, float subAge)
		{
			auto& slot = state.exhaustParticles[state.exhaustHead];
			const bool reuse = registry.isEntityValid(slot.entity);
			const Entity dotEntity = reuse ? slot.entity : registry.createEntity();

			auto& t = reuse ? registry.getComponent<Transform>(dotEntity) : registry.addComponent<Transform>(dotEntity);
			t.position = vec3(spawnPos, 0.0f);
			registry.setEntityDirty<Transform>(dotEntity, true);

			auto& dot = reuse ? registry.getComponent<Dot>(dotEntity) : registry.addComponent<Dot>(dotEntity);
			dot.materialId =
				services.materials2D().get(static_cast<uint16_t>(services.random().range<int>(1, matCount - 1))).id;

			auto& rb =
				reuse ? registry.getComponent<RigidBody2D>(dotEntity) : registry.addComponent<RigidBody2D>(dotEntity);
			rb.type = BodyType::Dynamic;
			rb.mass = 0.05f;
			rb.velocity = worldVel;
			rb.enableCollision = true;
			rb.enableShapeCollision = true;
			registry.setEntityDirty<RigidBody2D>(dotEntity, true);

			slot.entity = dotEntity;
			slot.lifetime = services.random().range(2.6f, 4.0f);
			state.exhaustHead = (state.exhaustHead + 1) % MAX_EXHAUST_PARTICLES;

			recordRecentExhaustSpawn(state, spawnPos, worldVel, subAge);
		}

		void syncShipClusterBodies(const State& state, Registry& registry, const vec2& forward, const vec2& right)
		{
			for (size_t i = 0; i < state.shipDotCount; ++i)
			{
				const Entity bodyEnt = state.shipDots[i].entity;
				if (!registry.isEntityValid(bodyEnt))
					continue;

				const vec2 local = state.shipDots[i].localOffset;
				const vec2 worldPos = state.shipPos + right * local.x + forward * local.y;

				auto& t = registry.getComponent<Transform>(bodyEnt);
				t.position = vec3(worldPos, 0.0f);
				registry.setEntityDirty<Transform>(bodyEnt, true);

				auto& rb = registry.getComponent<RigidBody2D>(bodyEnt);
				rb.velocity = state.shipVelocity;
				registry.setEntityDirty<RigidBody2D>(bodyEnt, true);
			}
		}
	} // namespace

	void emitExhaustParticles(Registry& registry, ServiceProvider& services, const vec2& prevShipPos,
							  float prevShipAngle, bool wasThrusting, float delta)
	{
		State& state = getState(registry);
		advanceRecentExhaustSpawns(state, delta);

		if (!state.isThrusting)
		{
			state.exhaustSpawnAccumulator = 0.0f;
			return;
		}

		const vec2 currentForward(std::sin(state.shipAngle), std::cos(state.shipAngle));
		const float shipSpeed = glm::length(state.shipVelocity);
		const float forwardSpeed = std::max(0.0f, glm::dot(state.shipVelocity, currentForward));
		const float highSpeedRatio = std::clamp((shipSpeed - 8.0f) / 52.0f, 0.0f, 1.0f);

		// Boost nozzle exit velocity and density as ship speed climbs so the stream stays tight and never stalls behind
		const float jetSpeed =
			std::clamp(std::max(glm::mix(40.0f, 88.0f, highSpeedRatio), forwardSpeed + 22.0f), 40.0f, 108.0f);

		constexpr float ROW_SPACING = 0.96f;
		constexpr float MIN_SPAWN_CLEARANCE_SQ = 1.02f; // > Dot diameter^2 (1.0^2)
		const float waveInterval = ROW_SPACING / jetSpeed;

		if (!wasThrusting)
		{
			state.exhaustSpawnAccumulator = waveInterval;
		}
		else
		{
			state.exhaustSpawnAccumulator = std::min(state.exhaustSpawnAccumulator + delta, waveInterval * 6.0f);
		}

		float angleDelta = state.shipAngle - prevShipAngle;
		while (angleDelta > glm::pi<float>())
		{
			angleDelta -= glm::two_pi<float>();
		}
		while (angleDelta < -glm::pi<float>())
		{
			angleDelta += glm::two_pi<float>();
		}

		uint16_t matCount = services.materials2D().getMaterialCount();
		if (matCount <= 1)
		{
			matCount = 16;
		}

		while (state.exhaustSpawnAccumulator >= waveInterval)
		{
			const float subAge = std::clamp(state.exhaustSpawnAccumulator - waveInterval, 0.0f, delta);
			const float tEmit = delta > 0.0f ? (1.0f - (subAge / delta)) : 1.0f;
			const float emitAngle = prevShipAngle + angleDelta * tEmit;
			const vec2 emitForward(std::sin(emitAngle), std::cos(emitAngle));
			const vec2 emitRight(std::cos(emitAngle), -std::sin(emitAngle));
			const vec2 emitShipPos = glm::mix(prevShipPos, state.shipPos, tEmit);
			const vec2 emitNozzleBase = emitShipPos + emitForward * state.thrusterLocalY;

			std::span<const ExhaustLane> lanes;
			if (state.exhaustStaggerSign > 0.0f)
			{
				lanes = WAVE_A_LANES;
			}
			else if (shipSpeed > 18.0f)
			{
				lanes = WAVE_B_HIGH_SPEED_LANES;
			}
			else
			{
				lanes = WAVE_B_BASE_LANES;
			}

			state.exhaustSpawnAccumulator -= waveInterval;
			state.exhaustStaggerSign = -state.exhaustStaggerSign;

			for (const ExhaustLane& lane : lanes)
			{
				const float lateralJitter = services.random().range(-0.02f, 0.02f);
				const float angleJitter = services.random().range(-0.015f, 0.015f);
				const float speedJitter = services.random().range(-0.25f, 0.25f);
				const float axialSpeed = jetSpeed + speedJitter;
				const vec2 exhaustVel = (-emitForward + emitRight * (lane.fanSlope + angleJitter)) * axialSpeed;
				const vec2 worldVel = state.shipVelocity + exhaustVel;
				const vec2 spawnPos = emitNozzleBase + emitRight * (lane.localX + lateralJitter) + worldVel * subAge;

				if (overlapsRecentExhaust(state, spawnPos, MIN_SPAWN_CLEARANCE_SQ))
					continue;

				spawnExhaustDot(state, registry, services, spawnPos, worldVel, matCount, subAge);
			}
		}
	}

	void shipPhysicsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = std::min(services.time().deltaTime(), 0.05f);
		if (delta <= 0.0f)
		{
			return;
		}

		const vec2 prevShipPos = state.shipPos;
		const float prevShipAngle = state.shipAngle;

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

		const bool wasThrusting = state.isThrusting;
		state.isThrusting = thrustHeld && (state.fuel > 0.0f);

		// Forward direction: angle 0 points up (+Y), positive angle rotates clockwise
		vec2 forward = vec2(std::sin(state.shipAngle), std::cos(state.shipAngle));
		vec2 right = vec2(std::cos(state.shipAngle), -std::sin(state.shipAngle));

		if (state.isThrusting)
		{
			state.shipVelocity += forward * (state.thrustPower * delta);
			state.fuel = std::max(0.0f, state.fuel - state.fuelBurnRate * delta);
		}

		// 3. Gravity & linear drag
		state.shipVelocity.y -= state.gravity * delta;
		state.shipVelocity *= std::clamp(1.0f - state.linearDrag * delta, 0.0f, 1.0f);

		// 4. Integrate position & depenetrate against terrain SDF
		state.shipPos += state.shipVelocity * delta;
		resolveShipTerrainCollisions(state, services, forward, right);

		// 5. Respawn / Refuel on 'R' key, gamepad North (Y button), or void fall
		vec2 emitStartShipPos = prevShipPos;
		float emitStartShipAngle = prevShipAngle;
		if (services.input().getKeyDown(Input::KeyCode::R) ||
			services.input().getGamepadButtonDown(Input::GamepadButton::North) || state.shipPos.y < -30.0f)
		{
			state.shipPos = vec2(0.0f, 6.0f);
			state.shipVelocity = vec2(0.0f, 0.0f);
			state.shipAngle = 0.0f;
			state.fuel = state.maxFuel;
			state.exhaustSpawnAccumulator = 0.0f;
			for (auto& recent : state.recentExhaustSpawns)
			{
				recent.age = 1.0f;
			}
			emitStartShipPos = state.shipPos;
			emitStartShipAngle = state.shipAngle;
			forward = vec2(0.0f, 1.0f);
			right = vec2(1.0f, 0.0f);
		}

		// Refuel slowly when resting stably near the start area
		if (std::abs(state.shipPos.x) < 5.0f && state.shipPos.y < 7.0f && glm::length(state.shipVelocity) < 1.0f)
		{
			state.fuel = std::min(state.maxFuel, state.fuel + 20.0f * delta);
		}

		// 6. Emit exhaust rigidbodies and update ship cluster transforms/velocities
		emitExhaustParticles(registry, services, emitStartShipPos, emitStartShipAngle, wasThrusting, delta);
		syncShipClusterBodies(state, registry, forward, right);
	}

	void exhaustParticleSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		for (auto& slot : state.exhaustParticles)
		{
			if (slot.lifetime > 0.0f && registry.isEntityValid(slot.entity))
			{
				slot.lifetime -= delta;
				if (slot.lifetime <= 0.0f)
				{
					slot.lifetime = 0.0f;

					// Park expired particle in pool to avoid synchronous physics entity destruction stalls
					auto& t = registry.getComponent<Transform>(slot.entity);
					t.position = vec3(-10000.0f, -10000.0f, 0.0f);
					registry.setEntityDirty<Transform>(slot.entity, true);

					auto& rb = registry.getComponent<RigidBody2D>(slot.entity);
					rb.type = BodyType::Fixed;
					rb.velocity = vec2(0.0f, 0.0f);
					rb.enableCollision = false;
					rb.enableShapeCollision = false;
					registry.setEntityDirty<RigidBody2D>(slot.entity, true);
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
