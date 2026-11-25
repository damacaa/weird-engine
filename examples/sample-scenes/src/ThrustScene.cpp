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

		// Zero global physics gravity and very low friction/damping for space environment
		services.physics().setGravity(0.0f);
		services.physics().setDamping(0.0005f);
		services.physics().setFriction(0.01f);

		const State& state = getState(registry);
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 6.0f, state.cameraZoom);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}

	void loadShipSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Fixed palette indices for HUD legibility (all 16 slots are owned by the scene)
		uint16_t matCount = services.materials2D().getMaterialCount();
		if (matCount == 0)
		{
			matCount = 16;
		}

		auto getPaletteMat = [&](uint16_t preferredIdx) -> Material2DHandle
		{
			uint16_t idx = static_cast<uint16_t>(std::min<int>(preferredIdx, static_cast<int>(matCount) - 1));
			return services.materials2D().get(idx);
		};

		state.fuelLabelMaterial = getPaletteMat(1); // White
		state.fuelMaterial = getPaletteMat(5);		// Green
		state.fuelBgMaterial = getPaletteMat(2);	// Steel gray track

		spawnShipCluster(state, registry, services);
	}

	namespace
	{
		struct FuelUiLayout
		{
			float labelX = 28.0f;
			float labelTopY = 0.0f;
			float barStartX = 0.0f;
			float barY = 0.0f;
			float barLength = 185.0f;
			float trackRadius = 7.5f;
			float fillRadius = 4.5f;
		};

		/// Computes non-overlapping top-left positions for the native 40px HUD text and pill fuel gauge.
		FuelUiLayout computeFuelUiLayout()
		{
			constexpr float MARGIN_X = 28.0f;
			constexpr float TOP_MARGIN = 22.0f;
			constexpr float LABEL_HEIGHT = 40.0f; // 4-row glyph height at default UI dotRadius=5.0
			constexpr float LABEL_TO_BAR_GAP = 14.0f;

			FuelUiLayout layout;
			const float screenH = static_cast<float>(Display::height > 0 ? Display::height : 720);
			layout.labelX = MARGIN_X;
			layout.labelTopY = screenH - TOP_MARGIN;
			layout.barStartX = MARGIN_X + layout.trackRadius;
			layout.barY = layout.labelTopY - LABEL_HEIGHT - LABEL_TO_BAR_GAP - layout.trackRadius;
			return layout;
		}

		/// Synchronizes the fuel gauge track, active fill bar, and label transform to current state and window size.
		void applyFuelUiLayout(const State& state, Registry& registry, const FuelUiLayout& layout)
		{
			const float fuelRatio = state.maxFuel > 0.0f ? std::clamp(state.fuel / state.maxFuel, 0.0f, 1.0f) : 0.0f;

			if (registry.isEntityValid(state.fuelBgLineEntity) &&
				registry.hasComponent<UIShape>(state.fuelBgLineEntity))
			{
				auto& bgUi = registry.getComponent<UIShape>(state.fuelBgLineEntity);
				bgUi.parameters[DefaultShapes::Line::StartX] = layout.barStartX;
				bgUi.parameters[DefaultShapes::Line::StartY] = layout.barY;
				bgUi.parameters[DefaultShapes::Line::EndX] = layout.barStartX + layout.barLength;
				bgUi.parameters[DefaultShapes::Line::EndY] = layout.barY;
				bgUi.parameters[DefaultShapes::Line::Width] = layout.trackRadius;
			}

			if (registry.isEntityValid(state.fuelLineEntity) && registry.hasComponent<UIShape>(state.fuelLineEntity))
			{
				auto& fuelUi = registry.getComponent<UIShape>(state.fuelLineEntity);
				if (fuelRatio > 0.001f)
				{
					fuelUi.parameters[DefaultShapes::Line::StartX] = layout.barStartX;
					fuelUi.parameters[DefaultShapes::Line::StartY] = layout.barY;
					fuelUi.parameters[DefaultShapes::Line::EndX] = layout.barStartX + layout.barLength * fuelRatio;
					fuelUi.parameters[DefaultShapes::Line::EndY] = layout.barY;
					fuelUi.parameters[DefaultShapes::Line::Width] = layout.fillRadius;
				}
				else
				{
					fuelUi.parameters[DefaultShapes::Line::StartX] = -1000.0f;
					fuelUi.parameters[DefaultShapes::Line::StartY] = -1000.0f;
					fuelUi.parameters[DefaultShapes::Line::EndX] = -1000.0f;
					fuelUi.parameters[DefaultShapes::Line::EndY] = -1000.0f;
					fuelUi.parameters[DefaultShapes::Line::Width] = 0.0f;
				}
			}

			if (registry.isEntityValid(state.fuelLabelEntity) &&
				registry.hasComponent<Transform>(state.fuelLabelEntity))
			{
				auto& t = registry.getComponent<Transform>(state.fuelLabelEntity);
				t.position = vec3(layout.labelX, layout.labelTopY, 0.0f);

				if (registry.hasComponent<UITextRenderer>(state.fuelLabelEntity))
				{
					auto& tr = registry.getComponent<UITextRenderer>(state.fuelLabelEntity);
					const char* expectedLabel = state.shipBroken ? "CRASHED - R" : "FUEL";
					if (tr.text != expectedLabel)
					{
						tr.text = expectedLabel;
						registry.setComponentDirty(tr);
					}
				}
			}
		}
	} // namespace

	void setupUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		const FuelUiLayout layout = computeFuelUiLayout();

		// Fuel bar track (background line in layer 0)
		state.fuelBgLineEntity = services.shapes().addUIShape(
			{.shapeId = DefaultShapes::Line,
			 .variables = {{DefaultShapes::Line::StartX, layout.barStartX},
						   {DefaultShapes::Line::StartY, layout.barY},
						   {DefaultShapes::Line::EndX, layout.barStartX + layout.barLength},
						   {DefaultShapes::Line::EndY, layout.barY},
						   {DefaultShapes::Line::Width, layout.trackRadius}},
			 .material = state.fuelBgMaterial,
			 .layer = 0});

		// Active fuel fill rendered above track (higher layer avoids track SDF shadowing fill)
		state.fuelLineEntity = services.shapes().addUIShape(
			{.shapeId = DefaultShapes::Line,
			 .variables = {{DefaultShapes::Line::StartX, layout.barStartX},
						   {DefaultShapes::Line::StartY, layout.barY},
						   {DefaultShapes::Line::EndX, layout.barStartX + layout.barLength},
						   {DefaultShapes::Line::EndY, layout.barY},
						   {DefaultShapes::Line::Width, layout.fillRadius}},
			 .material = state.fuelMaterial,
			 .layer = 1});

		// Fuel text label anchored strictly above the bar
		state.fuelLabelEntity = registry.createEntity();
		auto& t = registry.addComponent<Transform>(state.fuelLabelEntity);
		t.position = vec3(layout.labelX, layout.labelTopY, 0.0f);
		auto& tr = registry.addComponent<UITextRenderer>(state.fuelLabelEntity);
		tr.text = "FUEL";
		tr.material = state.fuelLabelMaterial.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;

		applyFuelUiLayout(state, registry, layout);
	}

	namespace
	{
		struct TerrainCollisionResult
		{
			bool collided = false;
			bool hardImpact = false;
			bool hitStar = false;
			Entity hitEntity = INVALID_ENTITY;
			vec2 impactPoint = vec2(0.0f);
			vec2 impactNormal = vec2(0.0f, 1.0f);
		};

		TerrainCollisionResult resolveShipTerrainCollisions(State& state, ServiceProvider& services,
															const vec2& forward, const vec2& right)
		{
			constexpr float PROBE_RADIUS = 0.50f;
			constexpr float BOUNCE_SPEED_THRESHOLD = 1.5f;
			constexpr float RESTITUTION = 0.45f;
			TerrainCollisionResult result;

			for (size_t i = 0; i < state.shipDotCount; ++i)
			{
				const vec2 local = state.shipDots[i].localOffset;
				const vec2 worldProbe = state.shipPos + right * local.x + forward * local.y;

				const RaymarchResult hit = services.physics().sampleAt(worldProbe, false);
				if (hit.entity == INVALID_ENTITY || hit.distance >= PROBE_RADIUS)
					continue;

				result.collided = true;
				vec2 normal = services.physics().sampleGradient(worldProbe, false);
				if (glm::length2(normal) <= 0.0001f)
					continue;

				normal = glm::normalize(normal);
				state.shipPos += normal * (PROBE_RADIUS - hit.distance);

				const float vn = glm::dot(state.shipVelocity, normal);
				const bool isStar = services.tags().hasTag(hit.entity, "star");
				if (isStar)
				{
					if (!result.hitStar)
					{
						result.hitStar = true;
						result.hitEntity = hit.entity;
						result.impactPoint = worldProbe - normal * hit.distance;
						result.impactNormal = normal;
					}
				}
				else if (!result.hitStar && vn < 0.0f && -vn >= state.crashSpeedThreshold)
				{
					result.hardImpact = true;
					result.hitEntity = hit.entity;
					result.impactPoint = worldProbe - normal * hit.distance;
					result.impactNormal = normal;
				}

				if (vn < 0.0f)
				{
					const float restitution = (-vn > BOUNCE_SPEED_THRESHOLD) ? RESTITUTION : 0.0f;
					state.shipVelocity -= (1.0f + restitution) * vn * normal;
				}
			}

			return result;
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
		constexpr float PHYSICS_DAMPING_APPROX = 0.05f; // Matches 0.0005f damping at 100 Hz

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

		void spawnPooledDot(State& state, Registry& registry, const vec2& spawnPos, const vec2& worldVel,
							uint16_t materialId, float lifetime, float mass = 0.05f)
		{
			auto& slot = state.exhaustParticles[state.exhaustHead];
			const bool reuse = registry.isEntityValid(slot.entity);
			const Entity dotEntity = reuse ? slot.entity : registry.createEntity();

			auto& t = reuse ? registry.getComponent<Transform>(dotEntity) : registry.addComponent<Transform>(dotEntity);
			t.position = vec3(spawnPos, 0.0f);
			registry.setEntityDirty<Transform>(dotEntity, true);

			auto& dot = reuse ? registry.getComponent<Dot>(dotEntity) : registry.addComponent<Dot>(dotEntity);
			dot.materialId = materialId;

			auto& rb =
				reuse ? registry.getComponent<RigidBody2D>(dotEntity) : registry.addComponent<RigidBody2D>(dotEntity);
			rb.type = BodyType::Dynamic;
			rb.mass = mass;
			rb.velocity = worldVel;
			rb.enableCollision = true;
			rb.enableShapeCollision = true;
			registry.setEntityDirty<RigidBody2D>(dotEntity, true);

			slot.entity = dotEntity;
			slot.lifetime = lifetime;
			state.exhaustHead = (state.exhaustHead + 1) % MAX_EXHAUST_PARTICLES;
		}

		void spawnExhaustDot(State& state, Registry& registry, ServiceProvider& services, const vec2& spawnPos,
							 const vec2& worldVel, uint16_t matCount, float subAge)
		{
			const uint16_t materialId =
				services.materials2D().get(static_cast<uint16_t>(services.random().range<int>(1, matCount - 1))).id;
			const float lifetime = services.random().range(2.6f, 4.0f);
			spawnPooledDot(state, registry, spawnPos, worldVel, materialId, lifetime);

			recordRecentExhaustSpawn(state, spawnPos, worldVel, subAge);
		}

		void spawnStarExplosion(State& state, Registry& registry, ServiceProvider& services, Entity starEntity,
								const vec2& surfacePoint, const vec2& outwardNormal)
		{
			constexpr size_t EXPLOSION_DOT_COUNT = 28;
			uint16_t starMaterial = 1;
			if (registry.isEntityValid(starEntity) && registry.hasComponent<Shape>(starEntity))
			{
				starMaterial = registry.getComponent<Shape>(starEntity).material;
			}

			const float baseAngle = std::atan2(outwardNormal.y, outwardNormal.x);
			for (size_t i = 0; i < EXPLOSION_DOT_COUNT; ++i)
			{
				const float spread = services.random().range(-1.15f, 1.15f);
				const float speed = services.random().range(18.0f, 46.0f);
				const float angle = baseAngle + spread;
				const vec2 dir(std::cos(angle), std::sin(angle));

				const vec2 lateral(-outwardNormal.y, outwardNormal.x);
				const float lateralJitter = services.random().range(-0.45f, 0.45f);
				const float outwardOffset = services.random().range(0.55f, 1.15f);
				const vec2 spawnPos = surfacePoint + outwardNormal * outwardOffset + lateral * lateralJitter;
				const vec2 worldVel = dir * speed;

				spawnPooledDot(state, registry, spawnPos, worldVel, starMaterial, services.random().range(2.2f, 3.8f),
							   0.12f);
			}
		}

		void breakShip(State& state, Registry& registry, ServiceProvider& services, const vec2& impactNormal)
		{
			if (state.shipBroken)
				return;

			state.shipBroken = true;
			state.isThrusting = false;

			auto isShipDot = [&](Entity entity)
			{
				for (size_t i = 0; i < state.shipDotCount; ++i)
				{
					if (state.shipDots[i].entity == entity)
						return true;
				}
				return false;
			};

			std::vector<Entity> constraintsToDestroy;
			registry.forEach<DistanceConstraint>(
				[&](Entity constraintEntity, const DistanceConstraint& c)
				{
					if (isShipDot(c.entityA) || isShipDot(c.entityB))
					{
						constraintsToDestroy.push_back(constraintEntity);
					}
				});

			registry.forEach<Spring>(
				[&](Entity springEntity, const Spring& s)
				{
					if (isShipDot(s.entityA) || isShipDot(s.entityB))
					{
						constraintsToDestroy.push_back(springEntity);
					}
				});

			ClusterInstance* cluster = services.clusters().getByName("spaceship");
			for (Entity ce : constraintsToDestroy)
			{
				if (cluster)
				{
					auto& ents = cluster->entities;
					ents.erase(std::remove(ents.begin(), ents.end(), ce), ents.end());
				}
				if (registry.isEntityValid(ce))
				{
					registry.destroyEntity(ce);
				}
			}

			const float baseAngle = std::atan2(impactNormal.y, impactNormal.x);
			for (size_t i = 0; i < state.shipDotCount; ++i)
			{
				const Entity e = state.shipDots[i].entity;
				if (!registry.isEntityValid(e) || !registry.hasComponent<RigidBody2D>(e))
					continue;

				auto& rb = registry.getComponent<RigidBody2D>(e);
				rb.type = BodyType::Dynamic;
				rb.enableCollision = true;
				rb.enableShapeCollision = true;

				const float jitterAngle = baseAngle + services.random().range(-1.2f, 1.2f);
				const vec2 scatterDir(std::cos(jitterAngle), std::sin(jitterAngle));
				const float scatterSpeed = services.random().range(4.0f, 14.0f);
				rb.velocity = state.shipVelocity * 0.45f + scatterDir * scatterSpeed;
				registry.setEntityDirty<RigidBody2D>(e, true);
			}
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

	/// Applies a linear deadzone and rescales remaining travel smoothly to [-1, 1].
	float applyStickDeadzone(float value, float deadzone)
	{
		const float absValue = std::abs(value);
		if (absValue <= deadzone || deadzone >= 1.0f)
		{
			return 0.0f;
		}
		const float sign = value > 0.0f ? 1.0f : -1.0f;
		return sign * std::clamp((absValue - deadzone) / (1.0f - deadzone), 0.0f, 1.0f);
	}

	/// Resolves ship rotation rate in [-1, 1] from keyboard, deadzoned left stick, and D-pad.
	float resolveRotationInput(float keyboardDir, float stickX, bool dpadLeft, bool dpadRight)
	{
		float rotateInput = std::clamp(keyboardDir, -1.0f, 1.0f);
		const float stickInput = applyStickDeadzone(stickX);
		if (stickInput != 0.0f)
		{
			rotateInput = stickInput;
		}
		const int dpadDir = (dpadRight ? 1 : 0) - (dpadLeft ? 1 : 0);
		if (dpadDir != 0)
		{
			rotateInput = static_cast<float>(dpadDir);
		}
		return std::clamp(rotateInput, -1.0f, 1.0f);
	}

	/// Resolves signed camera zoom rate in [-1, 1] (negative zooms in, positive zooms out).
	float resolveCameraZoomAxis(float rightStickY, float leftTrigger, bool leftShoulder)
	{
		float zoomAxis = applyStickDeadzone(rightStickY);
		if (leftShoulder)
		{
			zoomAxis -= 1.0f;
		}
		if (leftTrigger > 0.0f)
		{
			zoomAxis += applyStickDeadzone(leftTrigger);
		}
		return std::clamp(zoomAxis, -1.0f, 1.0f);
	}

	/// Advances smoothed camera zoom from mouse wheel steps and continuous/reset gamepad inputs.
	void updateCameraZoom(State& state, bool wheelUp, bool wheelDown, float zoomAxis, bool resetZoom, float delta)
	{
		constexpr float ZOOM_STEP_FACTOR = 1.15f;
		constexpr float GAMEPAD_ZOOM_RATE = 1.5f;
		constexpr float ZOOM_SPEED = 10.0f;

		if (resetZoom)
		{
			state.targetCameraZoom = DEFAULT_CAMERA_ZOOM;
		}
		if (wheelUp)
		{
			state.targetCameraZoom /= ZOOM_STEP_FACTOR;
		}
		if (wheelDown)
		{
			state.targetCameraZoom *= ZOOM_STEP_FACTOR;
		}
		if (zoomAxis != 0.0f && delta > 0.0f)
		{
			state.targetCameraZoom *= std::exp(zoomAxis * GAMEPAD_ZOOM_RATE * delta);
		}

		state.targetCameraZoom = std::clamp(state.targetCameraZoom, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
		if (delta > 0.0f)
		{
			state.cameraZoom = glm::mix(state.cameraZoom, state.targetCameraZoom, std::min(1.0f, ZOOM_SPEED * delta));
		}
	}

	/// Recharges fuel slowly in flight and faster while resting on terrain when thrust is released.
	void recoverFuel(State& state, bool thrustHeld, bool landed, float delta)
	{
		if (delta <= 0.0f || thrustHeld || state.isThrusting || state.fuel >= state.maxFuel)
		{
			return;
		}

		const bool restingOnPlanet = landed && glm::length(state.shipVelocity) < 1.0f;
		const float recoveryRate = restingOnPlanet ? state.landedFuelRecoveryRate : state.fuelRecoveryRate;
		state.fuel = std::min(state.maxFuel, state.fuel + recoveryRate * delta);
	}

	void spawnShipCluster(State& state, Registry& registry, ServiceProvider& services)
	{
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
			rb.velocity = vec2(0.0f);
			rb.enableCollision = false;		// Prevent cluster dot-to-dot collisions and nozzle snagging
			rb.enableShapeCollision = true; // Enable collisions with map terrain shapes
			registry.setEntityDirty<RigidBody2D>(e, true);

			state.shipDots[state.shipDotCount++] = {e, localOffset};
		}

		// Place exhaust emission point just behind the rear thruster pods
		state.thrusterLocalY = minLocalY - 0.36f;
	}

	void resetShip(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		state.shipPos = vec2(0.0f, 6.0f);
		state.shipVelocity = vec2(0.0f, 0.0f);
		state.shipAngle = 0.0f;
		state.fuel = state.maxFuel;
		state.isThrusting = false;
		state.exhaustSpawnAccumulator = 0.0f;
		for (auto& recent : state.recentExhaustSpawns)
		{
			recent.age = 1.0f;
		}

		if (state.shipBroken)
		{
			if (ClusterInstance* cluster = services.clusters().getByName("spaceship"))
			{
				services.clusters().destroyCluster(cluster);
			}
			spawnShipCluster(state, registry, services);
			state.shipBroken = false;
		}
		else
		{
			syncShipClusterBodies(state, registry, vec2(0.0f, 1.0f), vec2(1.0f, 0.0f));
		}
	}

	void stepShipPhysics(Registry& registry, ServiceProvider& services, float delta)
	{
		State& state = getState(registry);
		delta = std::min(delta, 0.05f);
		if (delta <= 0.0f)
		{
			return;
		}

		const vec2 prevShipPos = state.shipPos;
		const float prevShipAngle = state.shipAngle;

		const bool resetRequested = services.input().getKeyDown(Input::KeyCode::R) ||
									services.input().getGamepadButtonDown(Input::GamepadButton::North) ||
									services.input().getGamepadButtonDown(Input::GamepadButton::Start);
		if (resetRequested)
		{
			resetShip(registry, services);
			return;
		}

		if (state.shipBroken)
		{
			state.isThrusting = false;
			vec2 avgPos(0.0f);
			size_t activeDots = 0;
			for (size_t i = 0; i < state.shipDotCount; ++i)
			{
				const Entity e = state.shipDots[i].entity;
				if (!registry.isEntityValid(e) || !registry.hasComponent<Transform>(e))
					continue;

				avgPos += vec2(registry.getComponent<Transform>(e).position);
				++activeDots;
			}
			if (activeDots > 0)
			{
				state.shipPos = avgPos / static_cast<float>(activeDots);
			}
			return;
		}

		// 1. Rotation input (A/D, Left/Right arrow, gamepad LeftStick / Dpad)
		float keyboardRotate = 0.0f;
		if (services.input().getKey(Input::KeyCode::A) || services.input().getKey(Input::KeyCode::Left))
		{
			keyboardRotate -= 1.0f;
		}
		if (services.input().getKey(Input::KeyCode::D) || services.input().getKey(Input::KeyCode::Right))
		{
			keyboardRotate += 1.0f;
		}
		const float rotateInput =
			resolveRotationInput(keyboardRotate, services.input().getGamepadAxis(Input::GamepadAxis::LeftX),
								 services.input().getGamepadButton(Input::GamepadButton::DpadLeft),
								 services.input().getGamepadButton(Input::GamepadButton::DpadRight));

		state.shipAngle += rotateInput * state.rotationSpeed * delta;
		while (state.shipAngle > glm::pi<float>())
		{
			state.shipAngle -= glm::two_pi<float>();
		}
		while (state.shipAngle < -glm::pi<float>())
		{
			state.shipAngle += glm::two_pi<float>();
		}

		// 2. Thrust input (W, Up, Space, gamepad South / RightShoulder / RightTrigger)
		bool thrustHeld = services.input().getKey(Input::KeyCode::W) || services.input().getKey(Input::KeyCode::Up) ||
						  services.input().getKey(Input::KeyCode::Space) ||
						  services.input().getGamepadButton(Input::GamepadButton::South) ||
						  services.input().getGamepadButton(Input::GamepadButton::RightShoulder) ||
						  services.input().getGamepadAxis(Input::GamepadAxis::RightTrigger) > 0.2f;

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

		// 3. Gravity (toward nearest shape via SDF gradient) & linear drag
		const RaymarchResult nearestSurface = services.physics().sampleAt(state.shipPos, false);
		if (nearestSurface.entity != INVALID_ENTITY)
		{
			const vec2 gravityDir = -services.physics().sampleGradient(state.shipPos, false);
			state.shipVelocity += gravityDir * (state.gravity * delta);
		}
		state.shipVelocity *= std::clamp(1.0f - state.linearDrag * delta, 0.0f, 1.0f);

		// 4. Integrate position & depenetrate against terrain SDF
		state.shipPos += state.shipVelocity * delta;
		const TerrainCollisionResult collision = resolveShipTerrainCollisions(state, services, forward, right);

		if (collision.hitStar || collision.hardImpact)
		{
			syncShipClusterBodies(state, registry, forward, right);
			if (collision.hitStar)
			{
				spawnStarExplosion(state, registry, services, collision.hitEntity, collision.impactPoint,
								   collision.impactNormal);
			}
			breakShip(state, registry, services, collision.impactNormal);
			return;
		}

		// 5. Recover fuel passively while coasting, or faster when resting stably on a planet surface
		recoverFuel(state, thrustHeld, collision.collided, delta);

		// 6. Emit exhaust rigidbodies and update ship cluster transforms/velocities
		emitExhaustParticles(registry, services, prevShipPos, prevShipAngle, wasThrusting, delta);
		syncShipClusterBodies(state, registry, forward, right);
	}

	void shipPhysicsSystem(Registry& registry, ServiceProvider& services)
	{
		stepShipPhysics(registry, services, services.time().deltaTime());
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
		applyFuelUiLayout(getState(registry), registry, computeFuelUiLayout());
	}

	void cameraFollowSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		const float delta = std::min(services.time().deltaTime(), 0.05f);

		const float zoomAxis =
			resolveCameraZoomAxis(services.input().getGamepadAxis(Input::GamepadAxis::RightY),
								  services.input().getGamepadAxis(Input::GamepadAxis::LeftTrigger),
								  services.input().getGamepadButton(Input::GamepadButton::LeftShoulder));

		updateCameraZoom(state, services.input().getMouseButton(Input::MouseButton::WheelUp),
						 services.input().getMouseButton(Input::MouseButton::WheelDown), zoomAxis,
						 services.input().getGamepadButtonDown(Input::GamepadButton::RightStick), delta);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		constexpr float FOLLOW_SPEED = 5.0f;
		const float followAlpha = std::min(1.0f, FOLLOW_SPEED * delta);
		cameraTransform.position.x = glm::mix(cameraTransform.position.x, state.shipPos.x, followAlpha);
		cameraTransform.position.y = glm::mix(cameraTransform.position.y, state.shipPos.y, followAlpha);
		cameraTransform.position.z = state.cameraZoom;
	}
} // namespace ThrustSceneNamespace
