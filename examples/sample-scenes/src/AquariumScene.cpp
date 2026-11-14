#include "AquariumScene.h"

#include "weird-renderer/components/Light2DComponent.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <glm/gtc/constants.hpp>

using namespace WeirdEngine;

namespace WeirdEngine
{
	namespace AquariumSceneNamespace
	{
		namespace
		{
			constexpr std::array<vec2, VENT_COUNT> VENT_POSITIONS = {
				vec2(1.65f, -30.0f),  // Clam pearl micro-stream
				vec2(-41.2f, -25.4f), // Left sand seam clear of arch overhang
				vec2(42.5f, -10.8f)	  // Violet coral crest
			};

			constexpr std::array<const char*, 4> SPECIES_MAT_NAMES = {
				"fish_clown",
				"fish_tang",
				"fish_tetra",
				"fish_gold",
			};

			struct FishSnapshot
			{
				Entity entity = INVALID_ENTITY;
				vec2 pos{0.0f};
				vec2 vel{0.0f};
				FishSpecies species = FishSpecies::Clownfish;
			};

			struct FoodSnapshot
			{
				Entity entity = INVALID_ENTITY;
				vec2 pos{0.0f};
				size_t poolIdx = MAX_FOOD;
			};

			float lengthSq(const vec2& v)
			{
				return glm::dot(v, v);
			}

			vec2 safeNormalize(const vec2& v, const vec2& fallback = vec2(0.0f))
			{
				float len2 = lengthSq(v);
				if (len2 > 1e-6f && std::isfinite(len2))
				{
					return v * glm::inversesqrt(len2);
				}
				return fallback;
			}

			vec2 clampMagnitude(const vec2& v, float maxLen)
			{
				float len2 = lengthSq(v);
				if (len2 > maxLen * maxLen && len2 > 1e-6f)
				{
					return v * (maxLen * glm::inversesqrt(len2));
				}
				return v;
			}

			vec2 rotateVec(const vec2& v, float angle)
			{
				float c = std::cos(angle);
				float s = std::sin(angle);
				return vec2(c * v.x - s * v.y, s * v.x + c * v.y);
			}

			vec2 offscreenParkPos(size_t index, float baseY)
			{
				return vec2(-5000.0f - static_cast<float>(index % 8) * 5.0f,
							baseY - static_cast<float>(index / 8) * 5.0f);
			}

			State& getState(Registry& registry)
			{
				State* state = registry.getState<State>();
				WEIRD_ASSERT(state != nullptr, "AquariumScene State is missing: stateInitSystem must run first");
				return *state;
			}

			Material2DHandle resolveMaterial(ServiceProvider& services, const char* name, const vec4& primary,
											 const vec4& secondary, Pattern2D pattern, float scale)
			{
				Material2D& mat = services.materials2D().getOrCreate(name,
																	 [&](Material2D& m)
																	 {
																		 m.color = primary;
																		 m.secondaryColor = secondary;
																		 m.pattern = pattern;
																		 m.patternScale = scale;
																	 });
				return Material2DHandle(mat);
			}

			const char* buildCustomBackgroundShader()
			{
				return R"glsl(
vec3 getBackground(vec2 uv, vec2 worldPos)
{
	// Sleek dark gallery backdrop outside the beveled aquarium walls
	vec2 tankDistVec = abs(worldPos) - vec2(54.5, 35.5);
	float outerDist = length(max(tankDistVec, vec2(0.0)));
	if (outerDist > 0.0)
	{
		float vig = clamp(length(worldPos) * 0.011, 0.0, 1.0);
		vec3 roomColor = mix(vec3(0.032, 0.052, 0.088), vec3(0.010, 0.014, 0.024), vig);
		float glassGlow = exp(-outerDist * 0.09) * 0.14;
		roomColor += vec3(0.06, 0.42, 0.56) * glassGlow;
		float tableShadow = (worldPos.y < -37.5) ? smoothstep(-37.5, -50.0, worldPos.y) : 0.0;
		roomColor *= mix(1.0, 0.50, tableShadow);
		return roomColor;
	}

	// Dynamic rippled water level inside the tank
	float surfaceY = 28.5
		+ 0.40 * sin(worldPos.x * 0.16 + u_time * 1.6)
		+ 0.18 * cos(worldPos.x * 0.31 - u_time * 2.2);

	if (worldPos.y > surfaceY)
	{
		float dx = (worldPos.x - 4.0) * 0.030;
		float hoodGlow = exp(-dx * dx) * 0.25;
		float headGrad = clamp((worldPos.y - surfaceY) / 7.0, 0.0, 1.0);
		vec3 airCol = mix(vec3(0.07, 0.21, 0.31), vec3(0.020, 0.045, 0.085), headGrad);
		return airCol + vec3(0.18, 0.46, 0.58) * hoodGlow * (1.0 - headGrad * 0.7);
	}

	float depth = clamp((surfaceY - worldPos.y) / 63.0, 0.0, 1.0);
	float depthCurve = smoothstep(0.0, 1.0, depth);
	vec3 waterTop = u_bgPrimaryColor.rgb;
	vec3 waterBot = u_bgSecondaryColor.rgb;
	vec3 col = mix(waterTop, waterBot, depthCurve);

	// Soft broad overhead lamp bloom fading smoothly into the deeper water
	float beamDx = (worldPos.x - 4.0) * 0.025;
	float hoodScatter = exp(-beamDx * beamDx) * (1.0 - depthCurve * 0.85);
	col += vec3(0.05, 0.17, 0.21) * hoodScatter;

	// Soft distant reef ridges behind the glass for atmospheric depth
	float farReefY = -17.0 + 4.8 * sin(worldPos.x * 0.072 + 0.7) + 2.4 * cos(worldPos.x * 0.14 - 0.5);
	float farMask = smoothstep(farReefY + 1.4, farReefY - 1.2, worldPos.y);
	col = mix(col, vec3(0.014, 0.065, 0.145), farMask * 0.55);

	float midReefY = -22.5 + 3.8 * cos(worldPos.x * 0.098 - 0.3) + 1.8 * sin(worldPos.x * 0.20);
	float midMask = smoothstep(midReefY + 1.1, midReefY - 1.0, worldPos.y);
	col = mix(col, vec3(0.011, 0.052, 0.118), midMask * 0.65);

	// Crisp glowing meniscus band hugging the wave surface
	float surfaceDist = max(surfaceY - worldPos.y, 0.0);
	float meniscus = exp(-surfaceDist * 1.35);
	col += vec3(0.22, 0.58, 0.66) * meniscus * 0.42;

	return col * u_bgIntensity;
}
)glsl";
			}

			vec2 pushOutOfSolidSDF(ServiceProvider& services, const vec2& pos, float minClearance);

			void spawnBubbleFromPool(Registry& registry, ServiceProvider& services, State& state, const vec2& pos,
									 float riseSpeed, float wobbleAmp)
			{
				for (size_t attempt = 0; attempt < MAX_BUBBLES; ++attempt)
				{
					size_t idx = (state.nextBubbleCursor + attempt) % MAX_BUBBLES;
					Entity entity = state.bubblePool[idx];
					if (entity == INVALID_ENTITY || !registry.hasComponent<Bubble>(entity))
						continue;

					auto& bubble = registry.getComponent<Bubble>(entity);
					if (bubble.active)
						continue;

					state.nextBubbleCursor = (idx + 1) % MAX_BUBBLES;
					bubble.active = true;
					bubble.riseSpeed = riseSpeed;
					bubble.wobbleFreq = services.random().range(2.2f, 4.4f);
					bubble.wobbleAmp = wobbleAmp;
					bubble.phase = services.random().range(0.0f, glm::two_pi<float>());
					bubble.stuckTimer = 0.0f;

					auto& t = registry.getComponent<Transform>(entity);
					t.position = vec3(pushOutOfSolidSDF(services, pos, 0.9f), 0.0f);
					registry.setEntityDirty<Transform>(entity, true);

					auto& rb = registry.getComponent<RigidBody2D>(entity);
					rb.type = BodyType::Dynamic;
					rb.enableCollision = false;
					rb.enableShapeCollision = true;
					rb.velocity = vec2(services.random().range(-1.4f, 1.4f), riseSpeed * 0.55f);
					registry.setEntityDirty<RigidBody2D>(entity, true);
					return;
				}
			}

			void deactivateBubble(Registry& registry, size_t idx, Entity entity)
			{
				if (entity == INVALID_ENTITY)
					return;

				auto& bubble = registry.getComponent<Bubble>(entity);
				bubble.active = false;

				auto& t = registry.getComponent<Transform>(entity);
				t.position = vec3(offscreenParkPos(idx, -5000.0f), 0.0f);
				registry.setEntityDirty<Transform>(entity, true);

				auto& rb = registry.getComponent<RigidBody2D>(entity);
				rb.type = BodyType::Kinematic;
				rb.enableCollision = false;
				rb.enableShapeCollision = false;
				rb.velocity = vec2(0.0f);
				registry.setEntityDirty<RigidBody2D>(entity, true);
			}

			bool spawnFoodFromPool(Registry& registry, ServiceProvider& services, State& state, const vec2& pos,
								   const vec2& initialVel)
			{
				for (size_t attempt = 0; attempt < MAX_FOOD; ++attempt)
				{
					size_t idx = (state.nextFoodCursor + attempt) % MAX_FOOD;
					Entity entity = state.foodPool[idx];
					if (entity == INVALID_ENTITY || !registry.hasComponent<FishFood>(entity))
						continue;

					auto& food = registry.getComponent<FishFood>(entity);
					if (food.active)
						continue;

					state.nextFoodCursor = (idx + 1) % MAX_FOOD;
					food.active = true;
					food.age = 0.0f;
					food.driftPhase = services.random().range(0.0f, glm::two_pi<float>());

					auto& t = registry.getComponent<Transform>(entity);
					t.position = vec3(pos, 0.0f);
					registry.setEntityDirty<Transform>(entity, true);

					auto& rb = registry.getComponent<RigidBody2D>(entity);
					rb.type = BodyType::Dynamic;
					rb.enableCollision = false;
					rb.enableShapeCollision = true;
					rb.velocity = initialVel;
					registry.setEntityDirty<RigidBody2D>(entity, true);
					return true;
				}
				return false;
			}

			void deactivateFood(Registry& registry, size_t idx, Entity entity)
			{
				if (entity == INVALID_ENTITY)
					return;

				auto& food = registry.getComponent<FishFood>(entity);
				food.active = false;
				food.age = 0.0f;

				auto& t = registry.getComponent<Transform>(entity);
				t.position = vec3(offscreenParkPos(idx, -5200.0f), 0.0f);
				registry.setEntityDirty<Transform>(entity, true);

				auto& rb = registry.getComponent<RigidBody2D>(entity);
				rb.type = BodyType::Kinematic;
				rb.enableCollision = false;
				rb.enableShapeCollision = false;
				rb.velocity = vec2(0.0f);
				registry.setEntityDirty<RigidBody2D>(entity, true);
			}

			void triggerRipple(Registry& registry, State& state, const vec2& center, float duration = 0.6f)
			{
				state.rippleCenter = center;
				state.rippleDuration = duration;
				state.rippleTimer = duration;

				if (state.waterRippleEntity != INVALID_ENTITY && registry.hasComponent<Shape>(state.waterRippleEntity))
				{
					auto& shape = registry.getComponent<Shape>(state.waterRippleEntity);
					shape.parameters[DefaultShapes::CircleLine::PosX] = center.x;
					shape.parameters[DefaultShapes::CircleLine::PosY] = center.y;
					shape.parameters[DefaultShapes::CircleLine::Radius] = 0.8f;
					shape.parameters[DefaultShapes::CircleLine::Thickness] = 0.22f;
				}
			}

			vec2 pushOutOfSolidSDF(ServiceProvider& services, const vec2& pos, float minClearance)
			{
				vec2 result = pos;
				for (int step = 0; step < 3; ++step)
				{
					float dist = services.physics().sampleDistance(result, false);
					if (dist >= minClearance)
						break;

					vec2 grad = safeNormalize(services.physics().sampleGradient(result, false), vec2(0.0f, 1.0f));
					result += grad * (minClearance - dist + 0.1f);
				}
				result.x = std::clamp(result.x, SWIM_MIN_X, SWIM_MAX_X);
				result.y = std::clamp(result.y, SWIM_MIN_Y, WATERLINE_Y + 3.0f);
				return result;
			}
		} // namespace

		void stateInitSystem(Registry& registry, ServiceProvider& services)
		{
			registry.emplaceState<State>();
		}

		void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
		{
			services.debug().setDebugInput(false);
			services.debug().setDebugFly(false);

			State& state = getState(registry);

			resolveMaterial(services, "sand_bed", vec4(0.92f, 0.78f, 0.54f, 1.0f), vec4(0.76f, 0.59f, 0.36f, 1.0f),
							Pattern2D::PerlinNoise, 0.75f);
			resolveMaterial(services, "reef_rock", vec4(0.25f, 0.34f, 0.43f, 1.0f), vec4(0.15f, 0.22f, 0.30f, 1.0f),
							Pattern2D::PerlinNoise, 0.55f);
			state.glassMaterial = resolveMaterial(services, "tank_glass", vec4(0.45f, 0.86f, 0.98f, 0.28f),
												  vec4(0.82f, 0.98f, 1.0f, 0.45f), Pattern2D::Gradient, 0.15f);
			resolveMaterial(services, "tank_stand", vec4(0.10f, 0.13f, 0.19f, 1.0f), vec4(0.19f, 0.24f, 0.33f, 1.0f),
							Pattern2D::Gradient, 0.22f);
			state.coralPinkMaterial = resolveMaterial(services, "coral_pink", vec4(1.0f, 0.34f, 0.48f, 1.0f),
													  vec4(0.80f, 0.16f, 0.34f, 1.0f), Pattern2D::Waves, 0.85f);
			resolveMaterial(services, "coral_violet", vec4(0.66f, 0.40f, 0.96f, 1.0f), vec4(0.42f, 0.18f, 0.72f, 1.0f),
							Pattern2D::PerlinNoise, 0.90f);
			resolveMaterial(services, "coral_amber", vec4(1.0f, 0.74f, 0.24f, 1.0f), vec4(0.92f, 0.42f, 0.12f, 1.0f),
							Pattern2D::Gradient, 0.50f);
			state.kelpDeepMaterial = resolveMaterial(services, "kelp_deep", vec4(0.12f, 0.65f, 0.44f, 1.0f),
													 vec4(0.06f, 0.40f, 0.27f, 1.0f), Pattern2D::Waves, 0.45f);
			state.kelpBrightMaterial = resolveMaterial(services, "kelp_bright", vec4(0.38f, 0.88f, 0.48f, 1.0f),
													   vec4(0.16f, 0.58f, 0.34f, 1.0f), Pattern2D::Waves, 0.55f);

			state.speciesMaterials[0] = resolveMaterial(services, SPECIES_MAT_NAMES[0], vec4(1.0f, 0.46f, 0.14f, 1.0f),
														vec4(1.0f, 0.95f, 0.86f, 1.0f), Pattern2D::Waves, 1.6f);
			state.speciesMaterials[1] = resolveMaterial(services, SPECIES_MAT_NAMES[1], vec4(0.22f, 0.66f, 1.0f, 1.0f),
														vec4(0.65f, 0.96f, 1.0f, 1.0f), Pattern2D::Waves, 1.8f);
			state.speciesMaterials[2] = resolveMaterial(services, SPECIES_MAT_NAMES[2], vec4(0.32f, 0.98f, 0.86f, 1.0f),
														vec4(0.16f, 0.72f, 0.88f, 1.0f), Pattern2D::Waves, 2.0f);
			state.speciesMaterials[3] = resolveMaterial(services, SPECIES_MAT_NAMES[3], vec4(1.0f, 0.88f, 0.24f, 1.0f),
														vec4(1.0f, 0.58f, 0.12f, 1.0f), Pattern2D::Waves, 1.6f);

			state.bubbleMaterial = resolveMaterial(services, "bubble", vec4(0.80f, 0.97f, 1.0f, 0.22f),
												   vec4(1.0f, 1.0f, 1.0f, 0.34f), Pattern2D::None, 1.0f);
			state.foodMaterial = resolveMaterial(services, "food_flake", vec4(0.92f, 0.54f, 0.22f, 1.0f),
												 vec4(0.68f, 0.30f, 0.12f, 1.0f), Pattern2D::PerlinNoise, 1.4f);
			state.pearlMaterial = resolveMaterial(services, "pearl_ui", vec4(0.95f, 0.99f, 1.0f, 1.0f),
												  vec4(0.62f, 0.94f, 1.0f, 1.0f), Pattern2D::Gradient, 0.65f);

			// Deep oceanic background shader so bright coral reef and tropical fish pop cleanly
			auto& background = services.render().getBackground();
			background.type = BackgroundType::Custom;
			background.primaryColor = vec4(0.06f, 0.34f, 0.48f, 1.0f);
			background.secondaryColor = vec4(0.014f, 0.068f, 0.155f, 1.0f);
			background.intensity = 1.0f;
			background.parallax = 1.0f;
			background.customShaderCode = buildCustomBackgroundShader();

			services.physics().setGravity(0.0f);
			services.physics().setDamping(0.0f);
			services.physics().setFriction(0.01f);

			auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
			cameraTransform.position = vec3(0.0f, -1.0f, state.cameraZoom);
			cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
		}

		void setupKelpAndWaterSystem(Registry& registry, ServiceProvider& services)
		{
			using namespace SDF;
			State& state = getState(registry);

			Vec2Expr p = point();

			// Swaying kelp frond SDF anchored stiffly at sand clearings (var0..6)
			Vec2Expr q = translate(p, {var(0), var(1)});
			Expr height = max(var(2), 1.0f);
			Expr t = clamp(q.y / height, 0.0f, 1.0f);
			Expr sway = var(4) * (t * t) * sin(time() * 1.45f + var(5) - t * 3.1f) +
						0.35f * var(3) * sin(t * var(6) + var(5) + time() * 2.1f);
			Expr halfW = var(3) * (1.0f - 0.78f * (t * t)) * (0.72f + 0.28f * sin(t * 14.0f + var(5)));
			Expr dx = abs(q.x - sway) - halfW;
			Expr dy = abs(q.y - height * 0.5f) - height * 0.5f;
			Expr kelpExpr = length({max(dx, 0.0f), max(dy, 0.0f)}) + min(max(dx, dy), 0.0f);
			ShapeId kelpShapeId = services.shapes().registerSDF(kelpExpr);

			struct KelpPlacement
			{
				float x;
				float topY;
				float width;
				float amp;
				float phase;
				float freq;
				bool bright;
			};

			// Find exact local sand surface height so kelp bases sit minimally underground (~0.85u) behind sand
			auto findLocalSandSurfaceY = [&](float x)
			{
				float lo = -33.8f;
				float hi = -24.0f;
				if (services.physics().sampleDistance(vec2(x, lo), false) >= 0.0f)
					return -30.5f;

				for (int step = 0; step < 18; ++step)
				{
					float mid = 0.5f * (lo + hi);
					if (services.physics().sampleDistance(vec2(x, mid), false) < 0.0f)
					{
						lo = mid;
					}
					else
					{
						hi = mid;
					}
				}
				return 0.5f * (lo + hi);
			};

			constexpr std::array<KelpPlacement, 10> kelpPlacements = {{
				{-49.5f, -11.2f, 0.62f, 1.6f, 0.2f, 7.5f, false},
				{-46.8f, -3.5f, 0.72f, 2.2f, 1.1f, 6.2f, true},
				{-43.8f, -10.9f, 0.56f, 1.3f, 2.3f, 8.2f, false},
				{-6.2f, -15.5f, 0.52f, 1.2f, 3.1f, 8.4f, true},
				{5.8f, -13.1f, 0.60f, 1.5f, 1.8f, 7.0f, false},
				{8.4f, -8.2f, 0.68f, 1.9f, 2.7f, 6.4f, true},
				{19.6f, -12.9f, 0.56f, 1.4f, 4.0f, 7.2f, false},
				{22.0f, -16.3f, 0.50f, 1.1f, 0.7f, 8.0f, true},
				{48.8f, -5.8f, 0.68f, 1.8f, 1.4f, 6.8f, true},
				{51.0f, -3.2f, 0.72f, 2.1f, 2.9f, 6.0f, false},
			}};

			for (const auto& k : kelpPlacements)
			{
				float sandY = findLocalSandSurfaceY(k.x);
				float baseY = sandY - 0.85f;
				float height = std::max(k.topY - baseY, 4.0f);
				Material2DHandle mat = k.bright ? state.kelpBrightMaterial : state.kelpDeepMaterial;
				services.shapes().addShape(
					{.shapeId = kelpShapeId,
					 .variables = {k.x, baseY, height, k.width, k.amp, k.phase, k.freq},
					 .material = mat,
					 .combination = CombinationType::Addition,
					 .hasCollision = false,
					 .group = 0}); // Rendered before sand (group 2) so shallow roots stay occluded
			}

			// Undulating translucent waterline meniscus inside tank glass
			Expr waveY = var(1) + 0.40f * sin(p.x * 0.16f + time() * 1.6f) + 0.18f * cos(p.x * 0.31f - time() * 2.2f);
			Expr surfDx = abs(p.x) - var(0);
			Expr surfDy = abs(p.y - waveY) - var(2);
			Expr surfExpr = length({max(surfDx, 0.0f), max(surfDy, 0.0f)}) + min(max(surfDx, surfDy), 0.0f);
			ShapeId surfaceShapeId = services.shapes().registerSDF(surfExpr);

			services.shapes().addShape({.shapeId = surfaceShapeId,
										.variables = {TANK_HALF_W, WATERLINE_Y, 0.30f},
										.material = state.glassMaterial,
										.combination = CombinationType::Addition,
										.hasCollision = false,
										.group = 35});

			// Click / glass tap ripple indicator
			state.waterRippleEntity =
				services.shapes().addShape({.shapeId = DefaultShapes::CircleLine,
											.variables = {{DefaultShapes::CircleLine::PosX, -5000.0f},
														  {DefaultShapes::CircleLine::PosY, -5000.0f},
														  {DefaultShapes::CircleLine::Radius, 0.8f},
														  {DefaultShapes::CircleLine::Thickness, 0.0f}},
											.material = state.pearlMaterial,
											.combination = CombinationType::Addition,
											.hasCollision = false,
											.group = 36});

			// Sculpted parametric tropical fish SDF shared by all hero fish entities:
			// var(0..1)=pos, var(2..3)=heading, var(4)=scale, var(5)=wag, var(6)=upSign
			Vec2Expr delta = translate(p, {var(0), var(1)});
			Expr invScale = 1.0f / max(var(4), 0.25f);
			Expr u = (delta.x * var(2) + delta.y * var(3)) * invScale;
			Expr v = (-delta.x * var(3) + delta.y * var(2)) * invScale;

			Expr backT = clamp(-u * 0.75f, 0.0f, 1.5f);
			Expr vw = v - var(5) * backT * backT;
			Expr taper = 1.0f + clamp(-u * 0.45f, -0.25f, 0.55f);
			Expr dBody = length({u * 0.62f, vw * 1.32f * taper}) - 0.62f;

			Expr dTailTop = sdLine({u, vw}, {-0.72f, 0.0f}, {-1.48f, 0.54f}, 0.13f);
			Expr dTailBot = sdLine({u, vw}, {-0.72f, 0.0f}, {-1.48f, -0.54f}, 0.13f);
			Expr dTail = sdfSmoothUnion(dTailTop, dTailBot, 0.14f);

			Expr dDorsal = sdLine({u, vw}, {0.10f, 0.28f * var(6)}, {-0.48f, 0.72f * var(6)}, 0.13f);
			Expr fishLocal = sdfSmoothUnion(dBody, sdfSmoothUnion(dTail, dDorsal, 0.12f), 0.18f);
			state.fishShapeId = services.shapes().registerSDF(fishLocal * var(4));
		}

		void setupLightingSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);

			// 1. Crisp sunlit directional light keeping all coral & fish hues vivid
			{
				Entity sun = registry.createEntity();
				registry.addComponent<Transform>(sun).position = vec3(0.0f);
				auto& light = registry.addComponent<Light2DComponent>(sun);
				light.type = Light2DType::Directional;
				light.direction = safeNormalize(vec2(0.28f, 0.96f), vec2(0.0f, 1.0f));
				light.color = vec4(0.96f, 0.99f, 1.0f, 0.92f);
				light.castShadows = true;
			}

			// 2. Localized clam pearl glow (tight radius so nearby sand glows gently without muddying rocks)
			{
				state.pearlLightEntity = registry.createEntity();
				registry.addComponent<Transform>(state.pearlLightEntity).position = vec3(0.48f, -28.65f, 0.0f);
				auto& light = registry.addComponent<Light2DComponent>(state.pearlLightEntity);
				light.type = Light2DType::Point;
				light.color = vec4(0.45f, 0.98f, 1.0f, 0.42f);
				light.radius = 10.5f;
				light.castShadows = false;
			}
		}

		void setupPoolsAndFishSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);

			// Bubble pool
			for (size_t i = 0; i < MAX_BUBBLES; ++i)
			{
				Entity e = registry.createEntity();
				auto& t = registry.addComponent<Transform>(e);
				t.position = vec3(offscreenParkPos(i, -5000.0f), 0.0f);

				auto& dot = registry.addComponent<Dot>(e);
				dot.materialId = state.bubbleMaterial.id;

				auto& rb = registry.addComponent<RigidBody2D>(e);
				rb.mass = 0.25f;
				rb.type = BodyType::Kinematic;
				rb.enableCollision = false;
				rb.enableShapeCollision = false;

				auto& bubble = registry.addComponent<Bubble>(e);
				bubble.active = false;

				state.bubblePool[i] = e;
			}

			// Seed organic scattered bubbles at start above each vent crest
			for (size_t i = 0; i < 14; ++i)
			{
				const vec2& vent = VENT_POSITIONS[i % VENT_COUNT];
				float minY = std::min(vent.y + 0.8f, WATERLINE_Y - 4.0f);
				vec2 pos(vent.x + services.random().range(-1.4f, 1.4f),
						 services.random().range(minY, WATERLINE_Y - 2.0f));
				pos = pushOutOfSolidSDF(services, pos, 1.0f);
				spawnBubbleFromPool(registry, services, state, pos, services.random().range(5.8f, 9.8f),
									services.random().range(2.2f, 4.2f));
			}

			// Food pellet pool
			for (size_t i = 0; i < MAX_FOOD; ++i)
			{
				Entity e = registry.createEntity();
				auto& t = registry.addComponent<Transform>(e);
				t.position = vec3(offscreenParkPos(i, -5200.0f), 0.0f);

				auto& dot = registry.addComponent<Dot>(e);
				dot.materialId = state.foodMaterial.id;

				auto& rb = registry.addComponent<RigidBody2D>(e);
				rb.mass = 0.5f;
				rb.type = BodyType::Kinematic;
				rb.enableCollision = false;
				rb.enableShapeCollision = false;

				auto& food = registry.addComponent<FishFood>(e);
				food.active = false;

				state.foodPool[i] = e;
			}

			constexpr std::array<vec2, 4> schoolCenters = {
				vec2(-26.0f, 11.0f), // Clownfish
				vec2(22.0f, 9.0f),	 // Blue Tang / Chromis
				vec2(0.0f, 3.0f),	 // Neon Tetra
				vec2(-4.0f, 17.0f)	 // Sunburst Butterflyfish
			};

			state.fishCount = 0;

			// 16 Sculpted Hero Fish (parametric organic SDF shapes backed by RigidBody2D boids)
			for (size_t i = 0; i < HERO_FISH_COUNT; ++i)
			{
				uint8_t speciesIdx = static_cast<uint8_t>(i % 4);
				FishSpecies species = static_cast<FishSpecies>(speciesIdx);

				// High-contrast vivid tropical palette on deep sapphire water
				Material2DHandle mat = state.speciesMaterials[speciesIdx];
				if (species == FishSpecies::NeonTetra)
					mat = state.coralPinkMaterial; // Vivid coral anthias contrast against blue water

				const vec2 center = schoolCenters[speciesIdx];
				vec2 spawnPos(center.x + services.random().range(-10.0f, 10.0f),
							  center.y + services.random().range(-6.0f, 6.0f));
				spawnPos = pushOutOfSolidSDF(services, spawnPos, 2.2f);

				float angle = services.random().range(0.0f, glm::two_pi<float>());
				vec2 heading(std::cos(angle), std::sin(angle));

				float scale = services.random().range(1.12f, 1.50f);
				float cruise = 7.4f;
				float minSpd = 4.2f;
				float maxSpd = 12.6f;
				float swimFreq = 7.2f;
				if (species == FishSpecies::SunGold)
				{
					scale = services.random().range(1.22f, 1.58f);
					cruise = 6.2f;
					minSpd = 3.8f;
					maxSpd = 10.8f;
					swimFreq = 6.0f;
				}
				else if (species == FishSpecies::BlueTang)
				{
					cruise = 8.4f;
					swimFreq = 8.0f;
				}

				float upSign = std::clamp(heading.x * 4.0f, -1.0f, 1.0f);
				Entity shapeEnt = services.shapes().addShape(
					{.shapeId = state.fishShapeId,
					 .variables = {spawnPos.x, spawnPos.y, heading.x, heading.y, scale, 0.0f, upSign, 0.0f},
					 .material = mat,
					 .combination = CombinationType::Addition,
					 .hasCollision = false,
					 .group = Shape::GLOBAL_GROUP});

				Entity head = registry.createEntity();
				auto& t = registry.addComponent<Transform>(head);
				t.position = vec3(spawnPos, 0.0f);

				auto& rb = registry.addComponent<RigidBody2D>(head);
				rb.mass = 1.0f;
				rb.velocity = heading * cruise;
				rb.enableCollision = true;
				rb.enableShapeCollision = true;

				auto& fish = registry.addComponent<Fish>(head);
				fish.species = species;
				fish.shapeEntity = shapeEnt;
				fish.velocity = rb.velocity;
				fish.heading = heading;
				fish.cruiseSpeed = cruise + services.random().range(-0.6f, 0.6f);
				fish.minSpeed = minSpd;
				fish.maxSpeed = maxSpd;
				fish.swimPhase = services.random().range(0.0f, glm::two_pi<float>());
				fish.swimFreq = swimFreq;
				fish.visualScale = scale;
				fish.upSign = upSign;

				state.fishCount++;
			}

			// 14 Bright Shimmering Neon Minnow Dots weaving between the coral & reef arch
			for (size_t i = 0; i < MINNOW_COUNT; ++i)
			{
				FishSpecies species = (i % 2 == 0) ? FishSpecies::NeonTetra : FishSpecies::SunGold;
				Material2DHandle mat = (i % 2 == 0) ? state.speciesMaterials[2] : state.speciesMaterials[3];

				vec2 spawnPos(services.random().range(-22.0f, 22.0f), services.random().range(-6.0f, 18.0f));
				spawnPos = pushOutOfSolidSDF(services, spawnPos, 1.6f);

				float angle = services.random().range(0.0f, glm::two_pi<float>());
				vec2 heading(std::cos(angle), std::sin(angle));
				float cruise = services.random().range(9.2f, 11.4f);

				Entity tail = registry.createEntity();
				auto& tailT = registry.addComponent<Transform>(tail);
				tailT.position = vec3(spawnPos - heading * 0.55f, 0.0f);
				auto& tailDot = registry.addComponent<Dot>(tail);
				tailDot.materialId = mat.id;

				Entity head = registry.createEntity();
				auto& t = registry.addComponent<Transform>(head);
				t.position = vec3(spawnPos, 0.0f);
				auto& dot = registry.addComponent<Dot>(head);
				dot.materialId = mat.id;

				auto& rb = registry.addComponent<RigidBody2D>(head);
				rb.mass = 0.8f;
				rb.velocity = heading * cruise;
				rb.enableCollision = true;
				rb.enableShapeCollision = true;

				auto& fish = registry.addComponent<Fish>(head);
				fish.species = species;
				fish.tailEntity = tail;
				fish.velocity = rb.velocity;
				fish.heading = heading;
				fish.cruiseSpeed = cruise;
				fish.minSpeed = 6.2f;
				fish.maxSpeed = 14.8f;
				fish.swimPhase = services.random().range(0.0f, glm::two_pi<float>());
				fish.swimFreq = 11.5f;

				state.fishCount++;
			}
		}

		void cameraControlSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);

			constexpr float MIN_CAMERA_ZOOM = 20.0f;
			constexpr float MAX_CAMERA_ZOOM = 92.0f;
			constexpr float WHEEL_ZOOM_STEP = 5.0f;
			constexpr float GAMEPAD_ZOOM_SPEED = 45.0f;
			constexpr float TRIGGER_DEADZONE = 0.2f;

			if (services.input().getMouseButtonDown(Input::WheelUp))
			{
				state.targetCameraZoom -= WHEEL_ZOOM_STEP;
			}
			if (services.input().getMouseButtonDown(Input::WheelDown))
			{
				state.targetCameraZoom += WHEEL_ZOOM_STEP;
			}

			auto rescaleTrigger = [&](float value) -> float
			{
				if (value <= TRIGGER_DEADZONE)
					return 0.0f;
				return std::clamp((value - TRIGGER_DEADZONE) / (1.0f - TRIGGER_DEADZONE), 0.0f, 1.0f);
			};

			// L1/L2 zoom out, R1/R2 zoom in
			const float zoomOut =
				std::max(services.input().getGamepadButton(Input::GamepadButton::LeftShoulder) ? 1.0f : 0.0f,
						 rescaleTrigger(services.input().getGamepadAxis(Input::GamepadAxis::LeftTrigger)));
			const float zoomIn =
				std::max(services.input().getGamepadButton(Input::GamepadButton::RightShoulder) ? 1.0f : 0.0f,
						 rescaleTrigger(services.input().getGamepadAxis(Input::GamepadAxis::RightTrigger)));
			state.targetCameraZoom += (zoomOut - zoomIn) * GAMEPAD_ZOOM_SPEED * dt;
			state.targetCameraZoom = std::clamp(state.targetCameraZoom, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);

			state.cameraZoom += (state.targetCameraZoom - state.cameraZoom) * std::min(1.0f, dt * 10.0f);

			auto& camT = registry.getComponent<Transform>(services.render().getCameraEntity());
			camT.position.z = state.cameraZoom;

			float panSpeed = state.cameraZoom * 0.65f * dt;
			if (services.input().getKey(Input::A) || services.input().getKey(Input::Left))
				camT.position.x -= panSpeed;
			if (services.input().getKey(Input::D) || services.input().getKey(Input::Right))
				camT.position.x += panSpeed;
			if (services.input().getKey(Input::W) || services.input().getKey(Input::Up))
				camT.position.y += panSpeed;
			if (services.input().getKey(Input::S) || services.input().getKey(Input::Down))
				camT.position.y -= panSpeed;

			if (services.input().getKeyDown(Input::R))
			{
				camT.position.x = 0.0f;
				camT.position.y = -1.0f;
				state.targetCameraZoom = DEFAULT_CAMERA_ZOOM;
			}

			camT.position.x = std::clamp(camT.position.x, -45.0f, 45.0f);
			camT.position.y = std::clamp(camT.position.y, -30.0f, 30.0f);
		}

		void interactionSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);
			state.eatAudioCooldown = std::max(0.0f, state.eatAudioCooldown - dt);
			state.bubbleAudioCooldown = std::max(0.0f, state.bubbleAudioCooldown - dt);

			auto& camT = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(
				camT, vec2(services.input().getMouseX(), services.input().getMouseY()));

			bool feedPressed = services.input().getKeyDown(Input::E) ||
							   (services.input().getMouseButtonDown(Input::LeftClick) && !services.input().isUIClick());
			if (feedPressed)
			{
				vec2 dropCenter = pushOutOfSolidSDF(services, mouseWorld, 1.4f);
				bool droppedAny = false;
				for (int i = 0; i < 4; ++i)
				{
					vec2 offset(services.random().range(-2.2f, 2.2f), services.random().range(-0.8f, 1.4f));
					vec2 pos = pushOutOfSolidSDF(services, dropCenter + offset, 1.2f);
					vec2 vel(services.random().range(-1.4f, 1.4f), services.random().range(-2.0f, -0.5f));
					if (spawnFoodFromPool(registry, services, state, pos, vel))
					{
						droppedAny = true;
					}
				}

				if (droppedAny)
				{
					spawnBubbleFromPool(registry, services, state, dropCenter + vec2(-0.5f, 0.3f), 7.0f, 2.2f);
					spawnBubbleFromPool(registry, services, state, dropCenter + vec2(0.5f, 0.5f), 8.2f, 2.5f);
					triggerRipple(registry, state, dropCenter, 0.55f);
					services.audio().playSound({0.025f, services.random().range(620.0f, 780.0f), false, vec3(0.0f)});
				}
			}

			if (services.input().getMouseButtonDown(Input::RightClick) && !services.input().isUIClick())
			{
				vec2 tapPos(std::clamp(mouseWorld.x, -51.0f, 51.0f), std::clamp(mouseWorld.y, -31.0f, 31.0f));
				triggerRipple(registry, state, tapPos, 0.65f);
				services.audio().playSound({0.025f, services.random().range(380.0f, 460.0f), false, vec3(0.0f)});
			}

			if (services.input().getKeyDown(Input::Space))
			{
				for (size_t v = 0; v < VENT_COUNT; ++v)
				{
					for (int b = 0; b < 3; ++b)
					{
						vec2 ventPos = VENT_POSITIONS[v] +
									   vec2(services.random().range(-1.4f, 1.4f), services.random().range(0.0f, 1.8f));
						spawnBubbleFromPool(registry, services, state, ventPos, services.random().range(6.8f, 10.6f),
											services.random().range(2.2f, 4.0f));
					}
				}
				if (state.bubbleAudioCooldown <= 0.0f)
				{
					services.audio().playSound({0.02f, services.random().range(480.0f, 580.0f), false, vec3(0.0f)});
					state.bubbleAudioCooldown = 0.25f;
				}
			}
		}

		void updateLightingSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);
			float t = services.time().time();

			if (state.pearlLightEntity != INVALID_ENTITY &&
				registry.hasComponent<Light2DComponent>(state.pearlLightEntity))
			{
				auto& light = registry.getComponent<Light2DComponent>(state.pearlLightEntity);
				float pulse = 0.5f + 0.5f * std::sin(t * 2.1f);
				light.radius = 9.5f + 2.5f * pulse;
				light.color.a = 0.36f + 0.14f * pulse;
			}

			if (state.waterRippleEntity != INVALID_ENTITY && registry.hasComponent<Shape>(state.waterRippleEntity))
			{
				auto& shape = registry.getComponent<Shape>(state.waterRippleEntity);
				if (state.rippleTimer > 0.0f)
				{
					state.rippleTimer = std::max(0.0f, state.rippleTimer - dt);
				}

				if (state.rippleTimer > 0.0f)
				{
					float progress = 1.0f - (state.rippleTimer / std::max(state.rippleDuration, 0.001f));
					shape.parameters[DefaultShapes::CircleLine::PosX] = state.rippleCenter.x;
					shape.parameters[DefaultShapes::CircleLine::PosY] = state.rippleCenter.y;
					shape.parameters[DefaultShapes::CircleLine::Radius] = 0.6f + progress * 6.0f;
					shape.parameters[DefaultShapes::CircleLine::Thickness] = 0.22f * (1.0f - progress);
				}
				else if (shape.parameters[DefaultShapes::CircleLine::PosX] > -4000.0f)
				{
					shape.parameters[DefaultShapes::CircleLine::PosX] = -5000.0f;
					shape.parameters[DefaultShapes::CircleLine::PosY] = -5000.0f;
					shape.parameters[DefaultShapes::CircleLine::Radius] = 0.0f;
					shape.parameters[DefaultShapes::CircleLine::Thickness] = 0.0f;
				}
			}
		}

		void updateFoodSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);
			float now = services.time().time();
			state.activeFoodCount = 0;

			for (size_t i = 0; i < MAX_FOOD; ++i)
			{
				Entity e = state.foodPool[i];
				if (e == INVALID_ENTITY || !registry.hasComponent<FishFood>(e))
					continue;

				auto& food = registry.getComponent<FishFood>(e);
				if (!food.active)
					continue;

				food.age += dt;
				auto& t = registry.getComponent<Transform>(e);
				auto& rb = registry.getComponent<RigidBody2D>(e);

				if (food.age > 24.0f || t.position.y < TANK_BOTTOM_Y - 1.0f || std::abs(t.position.x) > TANK_HALF_W)
				{
					deactivateFood(registry, i, e);
					continue;
				}

				state.activeFoodCount++;

				bool inWater = t.position.y <= WATERLINE_Y;
				float gravityForce = inWater ? -3.2f : -14.0f;
				float dragFactor = inWater ? 2.4f : 0.3f;
				float currentX = inWater ? std::sin(now * 1.4f + t.position.y * 0.25f + food.driftPhase) * 2.2f : 0.0f;

				vec2 force = vec2(currentX, gravityForce) - rb.velocity * dragFactor;
				rb.pendingContinuousForce = clampMagnitude(force, 20.0f) * rb.mass;
			}
		}

		void updateFishFlockingSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);
			float now = services.time().time();

			std::array<FishSnapshot, MAX_FISH> fishSnap{};
			size_t fishCount = 0;

			registry.forEach<Fish, RigidBody2D, Transform>(
				[&](Entity e, Fish& fish, RigidBody2D& rb, Transform& t)
				{
					if (fishCount >= MAX_FISH)
						return;

					fish.velocity = rb.velocity;
					fish.boostTimer = std::max(0.0f, fish.boostTimer - dt);
					fishSnap[fishCount++] = {e, vec2(t.position), rb.velocity, fish.species};
				});

			std::array<FoodSnapshot, MAX_FOOD> foodSnap{};
			size_t foodCount = 0;
			for (size_t i = 0; i < MAX_FOOD; ++i)
			{
				Entity fe = state.foodPool[i];
				if (fe == INVALID_ENTITY)
					continue;
				const auto& food = registry.getComponent<FishFood>(fe);
				if (!food.active)
					continue;
				foodSnap[foodCount++] = {fe, vec2(registry.getComponent<Transform>(fe).position), i};
			}

			bool cursorRepelActive =
				services.input().getMouseButton(Input::RightClick) && !services.input().isUIClick();
			vec2 cursorWorld(0.0f);
			if (cursorRepelActive)
			{
				auto& camT = registry.getComponent<Transform>(services.render().getCameraEntity());
				cursorWorld = ECS::Camera::screenPositionToWorldPosition2D(
					camT, vec2(services.input().getMouseX(), services.input().getMouseY()));
			}

			constexpr float SEPARATION_RADIUS = 3.6f;
			constexpr float ALIGN_RADIUS_SAME = 13.0f;
			constexpr float COHESION_RADIUS_SAME = 15.5f;
			constexpr float ALIGN_RADIUS_OTHER = 6.5f;
			constexpr float FOOD_SEEK_RADIUS = 28.0f;
			constexpr float EAT_RADIUS = 1.65f;
			constexpr float OBSTACLE_LOOKAHEAD = 4.0f;
			constexpr float OBSTACLE_AVOID_MARGIN = 3.6f;
			constexpr float MAX_STEER_ACCEL = 26.0f;

			for (size_t i = 0; i < fishCount; ++i)
			{
				const Entity selfEntity = fishSnap[i].entity;
				const vec2 pos = fishSnap[i].pos;
				const vec2 vel = fishSnap[i].vel;
				const FishSpecies species = fishSnap[i].species;

				auto& fish = registry.getComponent<Fish>(selfEntity);
				auto& rb = registry.getComponent<RigidBody2D>(selfEntity);

				const vec2 currentHeading = safeNormalize(vel, fish.heading);

				vec2 separation(0.0f);
				vec2 alignmentSum(0.0f);
				float alignmentWeight = 0.0f;
				vec2 cohesionSum(0.0f);
				float cohesionWeight = 0.0f;

				for (size_t j = 0; j < fishCount; ++j)
				{
					if (i == j)
						continue;

					vec2 diff = pos - fishSnap[j].pos;
					float dist2 = lengthSq(diff);
					if (dist2 < 1e-6f)
					{
						separation += vec2(1.0f, 0.5f);
						continue;
					}

					float dist = std::sqrt(dist2);
					bool sameSpecies = (fishSnap[j].species == species);

					if (dist < SEPARATION_RADIUS)
					{
						float invDist = 1.0f / std::max(dist, 0.50f);
						separation += (diff * invDist) * ((SEPARATION_RADIUS - dist) * invDist);
					}

					if (sameSpecies)
					{
						if (dist < ALIGN_RADIUS_SAME)
						{
							float w = 1.0f - (dist / ALIGN_RADIUS_SAME);
							alignmentSum += fishSnap[j].vel * w;
							alignmentWeight += w;
						}
						if (dist < COHESION_RADIUS_SAME)
						{
							float w = 1.0f - (dist / COHESION_RADIUS_SAME) * 0.5f;
							cohesionSum += fishSnap[j].pos * w;
							cohesionWeight += w;
						}
					}
					else if (dist < ALIGN_RADIUS_OTHER)
					{
						float w = 0.14f * (1.0f - (dist / ALIGN_RADIUS_OTHER));
						alignmentSum += fishSnap[j].vel * w;
						alignmentWeight += w;
					}
				}

				vec2 desiredDir = currentHeading * 0.85f;

				if (lengthSq(separation) > 1e-6f)
				{
					desiredDir += safeNormalize(separation) * 2.6f;
				}
				if (alignmentWeight > 1e-4f)
				{
					vec2 avgVel = alignmentSum / alignmentWeight;
					if (lengthSq(avgVel) > 1e-6f)
						desiredDir += safeNormalize(avgVel) * 1.15f;
				}
				if (cohesionWeight > 1e-4f)
				{
					vec2 centerOfMass = cohesionSum / cohesionWeight;
					vec2 toCenter = centerOfMass - pos;
					if (lengthSq(toCenter) > 0.25f)
						desiredDir += safeNormalize(toCenter) * 0.85f;
				}

				// Graceful swimming wander + broad horizontal reef cruising
				float phase = static_cast<float>(i) * 0.93f;
				desiredDir.x += 0.34f * std::sin(now * 0.55f + phase);
				desiredDir.y += 0.18f * std::cos(now * 0.72f + phase * 1.3f);
				desiredDir += (vec2(0.0f, 3.0f) - pos) * 0.015f;

				bool seekingFood = false;
				if (foodCount > 0)
				{
					float nearestDist2 = FOOD_SEEK_RADIUS * FOOD_SEEK_RADIUS;
					vec2 nearestFoodPos(0.0f);
					size_t nearestSnapIdx = MAX_FOOD;

					for (size_t f = 0; f < foodCount; ++f)
					{
						vec2 toFood = foodSnap[f].pos - pos;
						float d2 = lengthSq(toFood);
						if (d2 < nearestDist2)
						{
							nearestDist2 = d2;
							nearestFoodPos = foodSnap[f].pos;
							nearestSnapIdx = f;
						}
					}

					if (nearestSnapIdx < foodCount)
					{
						if (nearestDist2 < EAT_RADIUS * EAT_RADIUS)
						{
							const auto& eaten = foodSnap[nearestSnapIdx];
							deactivateFood(registry, eaten.poolIdx, eaten.entity);
							foodSnap[nearestSnapIdx].pos = vec2(-5000.0f);
							fish.boostTimer = 0.5f;
							spawnBubbleFromPool(registry, services, state, pos + vec2(0.0f, 0.5f), 8.0f, 2.0f);
							if (state.eatAudioCooldown <= 0.0f)
							{
								services.audio().playSound(
									{0.02f, services.random().range(760.0f, 940.0f), false, vec3(0.0f)});
								state.eatAudioCooldown = 0.08f;
							}
						}
						else
						{
							vec2 toFoodDir = safeNormalize(nearestFoodPos - pos);
							desiredDir += toFoodDir * 2.8f;
							seekingFood = true;
						}
					}
				}

				if (cursorRepelActive)
				{
					vec2 away = pos - cursorWorld;
					float dist2 = lengthSq(away);
					if (dist2 < 18.0f * 18.0f && dist2 > 1e-4f)
					{
						float strength = 1.0f - std::sqrt(dist2) / 18.0f;
						desiredDir += safeNormalize(away) * (4.4f * strength);
						fish.boostTimer = std::max(fish.boostTimer, 0.25f);
					}
				}

				const std::array<vec2, 3> probes = {
					pos + currentHeading * OBSTACLE_LOOKAHEAD,
					pos + rotateVec(currentHeading, 0.42f) * (OBSTACLE_LOOKAHEAD * 0.85f),
					pos + rotateVec(currentHeading, -0.42f) * (OBSTACLE_LOOKAHEAD * 0.85f),
				};

				for (size_t p = 0; p < probes.size(); ++p)
				{
					float d = services.physics().sampleDistance(probes[p], false);
					if (d < OBSTACLE_AVOID_MARGIN)
					{
						vec2 grad =
							safeNormalize(services.physics().sampleGradient(probes[p], false), vec2(0.0f, 1.0f));
						float urgency = (OBSTACLE_AVOID_MARGIN - d) / OBSTACLE_AVOID_MARGIN;
						float weight = (p == 0 ? 4.8f : 2.6f) * (0.4f + urgency * urgency);

						vec2 tangent(-grad.y, grad.x);
						if (glm::dot(tangent, currentHeading) < 0.0f)
							tangent = -tangent;

						desiredDir += (grad * 0.80f + tangent * 0.45f) * weight;
					}
				}

				if (pos.x < SWIM_MIN_X)
					desiredDir.x += (SWIM_MIN_X - pos.x) * 1.6f;
				else if (pos.x > SWIM_MAX_X)
					desiredDir.x -= (pos.x - SWIM_MAX_X) * 1.6f;

				if (pos.y < SWIM_MIN_Y)
					desiredDir.y += (SWIM_MIN_Y - pos.y) * 1.6f;
				else if (pos.y > SWIM_MAX_Y)
					desiredDir.y -= (pos.y - SWIM_MAX_Y) * 2.2f;

				float targetSpeed = fish.cruiseSpeed;
				if (seekingFood)
					targetSpeed = std::min(fish.maxSpeed, fish.cruiseSpeed * 1.28f);
				if (fish.boostTimer > 0.0f)
					targetSpeed = fish.maxSpeed;

				vec2 desiredVel = safeNormalize(desiredDir, currentHeading) * targetSpeed;
				vec2 steer = clampMagnitude(desiredVel - vel, MAX_STEER_ACCEL);
				rb.pendingContinuousForce = steer * rb.mass;

				vec2 velDir = safeNormalize(vel, fish.heading);
				fish.heading = safeNormalize(glm::mix(fish.heading, velDir, std::min(1.0f, dt * 10.0f)), fish.heading);
				float targetUp = std::clamp(fish.heading.x * 4.0f, -1.0f, 1.0f);
				fish.upSign += (targetUp - fish.upSign) * std::min(1.0f, dt * 12.0f);
			}
		}

		void updateFishVisualsSystem(Registry& registry, ServiceProvider& services)
		{
			float dt = std::min(services.time().deltaTime(), 0.05f);

			registry.forEach<Fish, RigidBody2D, Transform>(
				[&](Entity e, Fish& fish, RigidBody2D& rb, Transform& t)
				{
					float speed = std::sqrt(lengthSq(rb.velocity));
					fish.swimPhase += dt * (fish.swimFreq + speed * 0.85f);

					vec2 heading = safeNormalize(fish.heading, vec2(1.0f, 0.0f));
					float wag = std::sin(fish.swimPhase);

					if (fish.shapeEntity != INVALID_ENTITY && registry.hasComponent<Shape>(fish.shapeEntity))
					{
						auto& shape = registry.getComponent<Shape>(fish.shapeEntity);
						shape.parameters[0] = t.position.x;
						shape.parameters[1] = t.position.y;
						shape.parameters[2] = heading.x;
						shape.parameters[3] = heading.y;
						shape.parameters[4] = fish.visualScale;
						shape.parameters[5] = wag * 0.26f;
						shape.parameters[6] = fish.upSign;
					}

					if (fish.tailEntity != INVALID_ENTITY && registry.hasComponent<Transform>(fish.tailEntity))
					{
						vec2 perp(-heading.y, heading.x);
						vec2 tailPos = vec2(t.position) - heading * 0.56f + perp * (wag * 0.18f);
						auto& tailT = registry.getComponent<Transform>(fish.tailEntity);
						tailT.position.x = tailPos.x;
						tailT.position.y = tailPos.y;
					}
				});
		}

		void updateBubblesSystem(Registry& registry, ServiceProvider& services)
		{
			State& state = getState(registry);
			float dt = std::min(services.time().deltaTime(), 0.05f);
			float now = services.time().time();

			for (size_t v = 0; v < VENT_COUNT; ++v)
			{
				state.ventTimers[v] -= dt;
				if (state.ventTimers[v] <= 0.0f)
				{
					float baseInterval = (v == 0) ? 0.65f : 0.45f;
					state.ventTimers[v] = baseInterval + services.random().range(0.0f, 0.45f);
					vec2 spawnPos = VENT_POSITIONS[v] + vec2(services.random().range(-0.9f, 0.9f), 0.3f);
					float riseSpeed =
						(v == 0) ? services.random().range(5.4f, 8.2f) : services.random().range(6.8f, 10.6f);
					spawnBubbleFromPool(registry, services, state, spawnPos, riseSpeed,
										services.random().range(2.0f, 4.2f));
				}
			}

			for (size_t i = 0; i < MAX_BUBBLES; ++i)
			{
				Entity e = state.bubblePool[i];
				if (e == INVALID_ENTITY || !registry.hasComponent<Bubble>(e))
					continue;

				auto& bubble = registry.getComponent<Bubble>(e);
				if (!bubble.active)
					continue;

				auto& t = registry.getComponent<Transform>(e);
				auto& rb = registry.getComponent<RigidBody2D>(e);

				if (t.position.y >= WATERLINE_Y - 0.3f || t.position.y < TANK_BOTTOM_Y - 2.0f ||
					std::abs(t.position.x) > TANK_HALF_W)
				{
					deactivateBubble(registry, i, e);
					continue;
				}

				float targetVx =
					std::sin(now * bubble.wobbleFreq + bubble.phase + t.position.y * 0.25f) * bubble.wobbleAmp;
				vec2 targetVel(targetVx, bubble.riseSpeed);

				// Steer around rock overhangs and cleanly pop bubbles caught underneath arches
				vec2 bubblePos(t.position);
				float solidDist = services.physics().sampleDistance(bubblePos, false);
				if (solidDist < 1.35f)
				{
					vec2 grad = safeNormalize(services.physics().sampleGradient(bubblePos, false), vec2(0.0f, 1.0f));
					float intoWall = glm::dot(targetVel, grad);
					if (intoWall < 0.0f)
					{
						targetVel -= grad * intoWall;
					}
					float slideSign = (rb.velocity.x != 0.0f) ? (rb.velocity.x > 0.0f ? 1.0f : -1.0f)
															  : (bubble.phase > glm::pi<float>() ? 1.0f : -1.0f);
					targetVel += grad * ((1.35f - solidDist) * 4.5f) + vec2(slideSign * bubble.riseSpeed * 0.65f, 0.0f);

					if (rb.velocity.y < 0.6f)
					{
						bubble.stuckTimer += dt;
						if (bubble.stuckTimer > 0.55f)
						{
							deactivateBubble(registry, i, e);
							continue;
						}
					}
					else
					{
						bubble.stuckTimer = 0.0f;
					}
				}
				else
				{
					bubble.stuckTimer = 0.0f;
				}

				vec2 force = clampMagnitude((targetVel - rb.velocity) * 5.0f, 18.0f);
				rb.pendingContinuousForce = force * rb.mass;
			}
		}
	} // namespace AquariumSceneNamespace
} // namespace WeirdEngine
