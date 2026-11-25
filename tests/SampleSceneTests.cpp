#include "InstrumentScene.h"
#include "MenuScene.h"
#include "MusicScene.h"
#include "RopeScene.h"
#include "TestHarness.h"
#include "TetrominoScene.h"
#include "ThrustScene.h"
#include "weird-engine/systems/ButtonSystem.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::InspectableScene;
using WeirdTests::require;
using WeirdTests::SnapshotFile;

namespace
{
	SceneManager& configureManager(bool threaded)
	{
		auto& manager = SceneManager::getInstance();
		PhysicsSettings settings;
		settings.runSimulationInThread = threaded;
		manager.setPhysicsSettings(settings);
		return manager;
	}

	void testRopeScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-rope-") + (threaded ? "threaded" : "single");

		manager.registerScene<InspectableScene<RopeScene>>(name);
		manager.loadScene(name);
		auto& scene = *static_cast<InspectableScene<RopeScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = RopeSceneNamespace::getState(registry);
		state.clampBalls = true;
		registry.forEach<RopeSceneNamespace::RopeBall>([](Entity, RopeSceneNamespace::RopeBall& ball)
													   { ball.shouldClamp = true; });
		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "RopeScene must support snapshots.");
		services.physics().forEachUserData([](SimulationID, BodyUserData& data)
										   { static_cast<RopeSceneNamespace::BallData&>(data).shouldClamp = false; });
		require(services.serialization().loadSnapshot(saved.path.string()), "RopeScene must restore its snapshot.");
		size_t restoredCount = 0;
		services.physics().forEachUserData(
			[&](SimulationID, BodyUserData& data)
			{
				require(data.type == RopeSceneNamespace::BallData::TYPE,
						"Restored rope data must have the correct type.");
				require(static_cast<RopeSceneNamespace::BallData&>(data).shouldClamp,
						"Restored balls must retain clamping.");
				++restoredCount;
			});
		require(restoredCount == 60, "All rope grid bodies must recover their user data.");

		// Fill a rope, then drag beyond its limit without opening a window.
		for (size_t i = 0; i < state.currentRopeBalls.capacity(); ++i)
		{
			const Entity ball = registry.createEntity();
			registry.addComponent<Transform>(ball);
			registry.addComponent<RigidBody2D>(ball);
			state.currentRopeBalls.push_back(ball);
		}
		state.creatingRope = true;
		state.startRopeBall = state.currentRopeBalls.front();
		state.lastSelectedBall = state.currentRopeBalls.back();
		state.startRopeMousePos = vec2(-1000.0f);
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		Input::SetMousePosition(400.0f, 400.0f);
		const Entity countBefore = registry.getEntityCount();
		for (int frame = 0; frame < 5; ++frame)
			RopeSceneNamespace::ropeCreationSystem(registry, services);
		require(registry.getEntityCount() == countBefore, "A full rope must not create untracked entities.");
		RopeSceneNamespace::cancelRopeCreation(registry, state);
		for (size_t i = 0; i < state.currentRopeBalls.capacity(); ++i)
		{
			state.currentRopeBalls.push_back(registry.createEntity());
			state.currentRopeSprings.push_back(registry.createEntity());
		}
		RopeSceneNamespace::cancelRopeCreation(registry, state);
		require(state.toDeleteRopeBalls.size() == 128, "Overlapping cancellations must retain every ball.");
		require(state.toDeleteRopeSprings.size() == 64, "Overlapping cancellations must retain every spring.");
	}

	void testTetrominoScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-tetromino-") + (threaded ? "threaded" : "single");

		manager.registerScene<InspectableScene<TetrominoScene>>(name);
		manager.loadScene(name);
		auto& scene = *static_cast<InspectableScene<TetrominoScene>*>(manager.getCurrentScene());
		auto& services = *scene.services;
		auto& state = TetrominoSceneNamespace::getState(*scene.registry);
		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "TetrominoScene must support snapshots.");
		state.isGameOver = true;
		state.isPaused = true;
		TetrominoSceneNamespace::g_isGameOverPhysics.store(true, std::memory_order_relaxed);
		services.physics().pause();
		services.physics().setGravity(-50.0f);
		services.physics().setDamping(0.5f);
		services.physics().setFriction(0.8f);
		require(services.serialization().loadSnapshot(saved.path.string()),
				"TetrominoScene must load its running state.");
		require(!state.isGameOver && !state.isPaused && !services.physics().isPaused(),
				"Running saves must resume physics.");
		require(!TetrominoSceneNamespace::g_isGameOverPhysics.load(), "The game-over callback flag must be restored.");
		require(services.physics().getGravity() == 0.0f, "Gravity must be restored.");
		require(services.physics().getDamping() == 0.004f, "Damping must be restored.");
		require(services.physics().getFriction() == 0.0f, "Friction must be restored.");

		state.isPaused = true;
		services.physics().pause();
		services.serialization().saveSnapshot(saved.path.string());
		state.isPaused = false;
		services.physics().resume();
		require(services.serialization().loadSnapshot(saved.path.string()),
				"TetrominoScene must load its paused state.");
		require(state.isPaused && services.physics().isPaused(), "Paused saves must keep physics paused.");
	}

	void testThrustScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-thrust-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<ThrustScene>>(name, ASSETS_PATH "platformer_world.weirdscene");
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<ThrustScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = ThrustSceneNamespace::getState(registry);

		require(state.shipDotCount == 17, "ThrustScene must instantiate all 17 spaceship cluster dots.");
		require(glm::length(state.shipPos - vec2(0.0f, 6.0f)) < 0.001f,
				"ThrustScene ship must initialize at spawn position (0, 6).");

		vec2 dotCentroid(0.0f);
		for (size_t i = 0; i < state.shipDotCount; ++i)
		{
			const Entity e = state.shipDots[i].entity;
			require(registry.isEntityValid(e) && registry.hasComponent<Transform>(e) && registry.hasComponent<Dot>(e) &&
						registry.hasComponent<RigidBody2D>(e),
					"Every spaceship cluster dot must retain Transform, Dot, and RigidBody2D.");

			dotCentroid += vec2(registry.getComponent<Transform>(e).position);
			const auto& rb = registry.getComponent<RigidBody2D>(e);
			require(rb.type == BodyType::Kinematic, "Ship cluster rigidbodies must be Kinematic.");
			require(!rb.enableCollision, "Ship cluster rigidbodies must disable body-body collision.");
			require(rb.enableShapeCollision, "Ship cluster rigidbodies must enable terrain shape collision.");
		}
		dotCentroid /= static_cast<float>(state.shipDotCount);
		require(glm::length(dotCentroid - state.shipPos) < 0.01f,
				"Spaceship cluster dots must remain centered on shipPos.");

		// Verify Fuel UI layers are separated (active fill draws over track) and label clears the bar vertically.
		require(registry.isEntityValid(state.fuelBgLineEntity) && registry.isEntityValid(state.fuelLineEntity) &&
					registry.isEntityValid(state.fuelLabelEntity),
				"ThrustScene fuel UI entities must all be created.");
		const auto& bgUi = registry.getComponent<UIShape>(state.fuelBgLineEntity);
		const auto& fuelUi = registry.getComponent<UIShape>(state.fuelLineEntity);
		const auto& labelTransform = registry.getComponent<Transform>(state.fuelLabelEntity);
		const float trackTop =
			bgUi.parameters[DefaultShapes::Line::StartY] + bgUi.parameters[DefaultShapes::Line::Width];
		const float labelBottom = labelTransform.position.y - 40.0f;
		require(bgUi.layer < fuelUi.layer,
				"Fuel bar fill must use a higher SDF layer than its track so it is not masked.");
		require(labelBottom > trackTop + 4.0f, "FUEL label must sit cleanly above the fuel bar without overlap.");
		require(bgUi.material != fuelUi.material, "Fuel fill and background track must use distinct materials.");

		// Verify slow passive fuel recovery in flight, pause under held thrust, and faster landed recharge.
		state.isThrusting = false;
		state.fuel = 40.0f;
		ThrustSceneNamespace::recoverFuel(state, true, false, 1.0f);
		require(std::abs(state.fuel - 40.0f) < 0.001f, "Fuel must not recover while thrust input is held.");
		ThrustSceneNamespace::recoverFuel(state, false, false, 1.0f);
		require(state.fuel > 40.5f && state.fuel < 50.0f,
				"Fuel must recover slowly during coasting flight when thrust is released.");
		const float midairRecovered = state.fuel;
		state.shipVelocity = vec2(0.0f, 0.0f);
		ThrustSceneNamespace::recoverFuel(state, false, true, 1.0f);
		require(state.fuel - midairRecovered > state.fuelRecoveryRate + 5.0f,
				"Landed resting fuel recovery must remain faster than mid-air passive recovery.");
		state.fuel = state.maxFuel;

		// Verify gamepad deadzone smoothing, D-pad override/cancellation, and camera zoom controls.
		require(ThrustSceneNamespace::applyStickDeadzone(0.15f) == 0.0f,
				"Stick deflection within deadzone must zero out.");
		require(std::abs(ThrustSceneNamespace::applyStickDeadzone(0.6f) - 0.5f) < 0.001f,
				"Stick deflection past deadzone must rescale smoothly.");
		require(ThrustSceneNamespace::resolveRotationInput(0.0f, 0.6f, true, false) == -1.0f,
				"Gamepad D-pad must override stick rotation input.");
		require(std::abs(ThrustSceneNamespace::resolveRotationInput(0.0f, 0.6f, true, true) - 0.5f) < 0.001f,
				"Simultaneous DpadLeft and DpadRight must cancel and fall back cleanly.");

		state.targetCameraZoom = ThrustSceneNamespace::DEFAULT_CAMERA_ZOOM;
		const float zoomInAxis = ThrustSceneNamespace::resolveCameraZoomAxis(-1.0f, 0.0f, false);
		require(zoomInAxis < -0.9f, "Right stick up must zoom camera in.");
		ThrustSceneNamespace::updateCameraZoom(state, false, false, zoomInAxis, false, 0.05f);
		require(state.targetCameraZoom < ThrustSceneNamespace::DEFAULT_CAMERA_ZOOM,
				"Negative zoom axis must decrease targetCameraZoom.");

		const float zoomOutAxis = ThrustSceneNamespace::resolveCameraZoomAxis(0.0f, 1.0f, false);
		require(zoomOutAxis > 0.9f, "Left trigger must zoom camera out.");
		ThrustSceneNamespace::updateCameraZoom(state, false, false, zoomOutAxis, false, 0.1f);
		require(state.targetCameraZoom > ThrustSceneNamespace::DEFAULT_CAMERA_ZOOM,
				"Positive zoom axis must increase targetCameraZoom.");

		ThrustSceneNamespace::updateCameraZoom(state, false, false, 0.0f, true, 0.05f);
		require(std::abs(state.targetCameraZoom - ThrustSceneNamespace::DEFAULT_CAMERA_ZOOM) < 0.001f,
				"Gamepad zoom reset must restore default camera zoom.");

		// Verify ThrustScene snapshot save & reload round-trip.
		const vec2 savedShipPos = state.shipPos;
		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "ThrustScene must support snapshots.");
		state.shipPos = vec2(-50.0f, 50.0f);
		state.shipDotCount = 0;
		require(services.serialization().loadSnapshot(saved.path.string()), "ThrustScene must restore its snapshot.");
		require(state.shipDotCount == 17, "Restored ThrustScene snapshot must keep all 17 ship cluster dots.");
		require(glm::length(state.shipPos - savedShipPos) < 0.001f, "Restored ThrustScene snapshot must keep shipPos.");

		// Verify high-throughput non-overlapping exhaust rigidbody spawning at low and high speeds.
		auto runExhaustFrames = [&](const vec2& velocity, float angularRate, int frames) -> size_t
		{
			constexpr float STEP_DT = 1.0f / 60.0f;
			state.shipPos = vec2(0.0f, 30.0f);
			state.shipVelocity = velocity;
			state.shipAngle = 0.0f;
			state.exhaustSpawnAccumulator = 0.0f;
			state.isThrusting = true;
			for (auto& recent : state.recentExhaustSpawns)
			{
				recent.age = 1.0f;
			}

			const size_t startHead = state.exhaustHead;
			for (int f = 0; f < frames; ++f)
			{
				const vec2 prevPos = state.shipPos;
				const float prevAngle = state.shipAngle;
				state.shipPos += velocity * STEP_DT;
				state.shipAngle += angularRate * STEP_DT;

				ThrustSceneNamespace::emitExhaustParticles(registry, services, prevPos, prevAngle, f > 0, STEP_DT);

				for (size_t i = 0; i < ThrustSceneNamespace::MAX_RECENT_EXHAUST_SPAWNS; ++i)
				{
					const auto& a = state.recentExhaustSpawns[i];
					if (a.age >= 0.25f)
						continue;
					for (size_t j = i + 1; j < ThrustSceneNamespace::MAX_RECENT_EXHAUST_SPAWNS; ++j)
					{
						const auto& b = state.recentExhaustSpawns[j];
						if (b.age >= 0.25f)
							continue;
						const vec2 d = a.pos - b.pos;
						const float distSq = d.x * d.x + d.y * d.y;
						require(distSq >= 0.99f, "Newly emitted exhaust rigidbodies must not overlap.");
					}
				}
			}

			return (state.exhaustHead + ThrustSceneNamespace::MAX_EXHAUST_PARTICLES - startHead) %
				   ThrustSceneNamespace::MAX_EXHAUST_PARTICLES;
		};

		const size_t lowSpeedCount = runExhaustFrames(vec2(0.0f, 0.0f), 0.0f, 30);
		const size_t highSpeedCount = runExhaustFrames(vec2(30.0f, 65.0f), 1.5f, 30);
		require(lowSpeedCount >= 45, "Low-speed thrust must push dense exhaust rigidbodies.");
		require(highSpeedCount > lowSpeedCount * 2,
				"High-speed flight must push significantly more exhaust rigidbodies.");

		// Verify snapshot serialization with active pooled exhaust rigidbodies in flight.
		require(services.serialization().loadSnapshot(saved.path.string()),
				"ThrustScene must recover baseline snapshot after high-speed exhaust bursts.");

		// Load ThrustScene with space.weirdscene to exercise terrain/star SDF crash and reset physics
		const std::string spaceSceneName = name + "-space";
		manager.registerScene<InspectableScene<ThrustScene>>(spaceSceneName, ASSETS_PATH "space.weirdscene");
		manager.loadScene(spaceSceneName);
		auto& spaceScene = *static_cast<InspectableScene<ThrustScene>*>(manager.getCurrentScene());
		Registry& spaceRegistry = *spaceScene.registry;
		ServiceProvider& spaceServices = *spaceScene.services;
		auto& spaceState = ThrustSceneNamespace::getState(spaceRegistry);

		auto countShipConstraints = [&]()
		{
			size_t count = 0;
			auto isShipDot = [&](Entity e)
			{
				for (size_t i = 0; i < spaceState.shipDotCount; ++i)
				{
					if (spaceState.shipDots[i].entity == e)
						return true;
				}
				return false;
			};
			spaceRegistry.forEach<DistanceConstraint>(
				[&](Entity, const DistanceConstraint& c)
				{
					if (isShipDot(c.entityA) || isShipDot(c.entityB))
					{
						++count;
					}
				});
			return count;
		};

		require(!spaceState.shipBroken, "Ship must start intact.");
		const size_t initialConstraints = countShipConstraints();
		require(initialConstraints > 0, "Intact spaceship cluster must own DistanceConstraints.");

		// Find lowest local dot offset so we can position the ship right above a surface cleanly
		float lowestLocalY = 0.0f;
		for (size_t i = 0; i < spaceState.shipDotCount; ++i)
		{
			lowestLocalY = std::min(lowestLocalY, spaceState.shipDots[i].localOffset.y);
		}
		const float topPlanetSurfaceY = 0.0f; // space.weirdscene planet 1: center (0, -30), radius 30

		// Soft landing below crashSpeedThreshold must keep ship intact
		spaceState.shipPos = vec2(0.0f, topPlanetSurfaceY - lowestLocalY + 0.35f);
		spaceState.shipVelocity = vec2(0.0f, -5.0f);
		ThrustSceneNamespace::stepShipPhysics(spaceRegistry, spaceServices, 0.016f);
		ThrustSceneNamespace::uiSystem(spaceRegistry, spaceServices);
		require(!spaceState.shipBroken && countShipConstraints() == initialConstraints,
				"Soft planet contact must keep ship constraints intact.");

		// Hard planet impact must destroy ship constraints, convert dots to Dynamic, and require reset
		spaceState.shipPos = vec2(0.0f, topPlanetSurfaceY - lowestLocalY + 0.35f);
		spaceState.shipVelocity = vec2(0.0f, -30.0f);
		ThrustSceneNamespace::stepShipPhysics(spaceRegistry, spaceServices, 0.016f);
		spaceRegistry.freeRemovedComponents();
		ThrustSceneNamespace::uiSystem(spaceRegistry, spaceServices);
		require(spaceState.shipBroken, "Hard planet impact must mark ship as broken.");
		require(countShipConstraints() == 0, "Hard planet impact must remove all ship DistanceConstraints.");
		ThrustSceneNamespace::stepShipPhysics(spaceRegistry, spaceServices, 0.016f);
		for (size_t i = 0; i < spaceState.shipDotCount; ++i)
		{
			const auto& rb = spaceRegistry.getComponent<RigidBody2D>(spaceState.shipDots[i].entity);
			require(rb.type == BodyType::Dynamic && rb.enableCollision,
					"Broken ship dots must become Dynamic colliding bodies.");
			require(glm::length2(rb.pendingContinuousForce) == 0.0f,
					"Broken ship dots must not receive gravity forces.");
		}
		require(spaceRegistry.getComponent<UITextRenderer>(spaceState.fuelLabelEntity).text == "CRASHED - R",
				"Fuel label must prompt player to reset when ship is broken.");

		// Resetting rebuilds the intact ship cluster
		ThrustSceneNamespace::resetShip(spaceRegistry, spaceServices);
		spaceRegistry.freeRemovedComponents();
		ThrustSceneNamespace::uiSystem(spaceRegistry, spaceServices);
		require(!spaceState.shipBroken && countShipConstraints() == initialConstraints,
				"Resetting a broken ship must restore intact cluster and constraints.");
		require(spaceRegistry.getComponent<UITextRenderer>(spaceState.fuelLabelEntity).text == "FUEL",
				"Fuel label must revert to FUEL after reset.");

		// Locate the star shape in space.weirdscene, tag it "star", and verify star-hit explosion
		Entity starEntity = INVALID_ENTITY;
		uint16_t starMaterial = 0;
		vec2 starCenter(0.0f);
		float starRadius = 0.0f;
		spaceRegistry.forEach<Shape>(
			[&](Entity e, const Shape& s)
			{
				if (s.distanceFieldId == static_cast<ShapeId>(DefaultShapes::Star))
				{
					starEntity = e;
					starMaterial = s.material;
					starCenter = vec2(s.parameters[0], s.parameters[1]);
					starRadius = s.parameters[2];
				}
			});
		require(starEntity != INVALID_ENTITY, "space.weirdscene must contain a Star shape.");
		spaceServices.tags().addTag(starEntity, "star");

		// Locate a point just outside the top of the star SDF via sampleAt
		vec2 probePt = starCenter + vec2(0.0f, starRadius + 40.0f);
		for (int s = 0; s < 400; ++s)
		{
			const RaymarchResult hit = spaceServices.physics().sampleAt(probePt, false);
			if (hit.entity == starEntity && hit.distance < 0.35f)
			{
				break;
			}
			probePt.y -= 0.5f;
		}
		require(spaceServices.physics().sampleAt(probePt, false).entity == starEntity,
				"Must locate Star surface point for star-collision check.");

		const size_t headBeforeStar = spaceState.exhaustHead;
		spaceState.shipPos = vec2(probePt.x, probePt.y - lowestLocalY);
		spaceState.shipVelocity = vec2(0.0f, -3.0f); // Low speed: star hit breaks regardless of speed
		ThrustSceneNamespace::stepShipPhysics(spaceRegistry, spaceServices, 0.016f);
		spaceRegistry.freeRemovedComponents();
		require(spaceState.shipBroken, "Touching a 'star'-tagged shape must break the ship even at low speed.");
		require(countShipConstraints() == 0, "Star impact must remove all ship DistanceConstraints.");

		size_t starExplosionCount = 0;
		const size_t spawnedDelta =
			(spaceState.exhaustHead + ThrustSceneNamespace::MAX_EXHAUST_PARTICLES - headBeforeStar) %
			ThrustSceneNamespace::MAX_EXHAUST_PARTICLES;
		for (size_t k = 0; k < spawnedDelta; ++k)
		{
			const size_t idx = (headBeforeStar + k) % ThrustSceneNamespace::MAX_EXHAUST_PARTICLES;
			const Entity dotEnt = spaceState.exhaustParticles[idx].entity;
			if (!spaceRegistry.isEntityValid(dotEnt) || !spaceRegistry.hasComponent<Dot>(dotEnt) ||
				!spaceRegistry.hasComponent<RigidBody2D>(dotEnt))
				continue;
			if (spaceRegistry.getComponent<Dot>(dotEnt).materialId == starMaterial &&
				glm::length(spaceRegistry.getComponent<RigidBody2D>(dotEnt).velocity) > 5.0f)
			{
				++starExplosionCount;
			}
		}
		require(starExplosionCount >= 20,
				"Star impact must spawn explosion Dot particles using the star shape's material.");
	}

	void testMusicScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-music-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<MusicScene>>(name);
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<MusicScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = MusicSceneNamespace::getState(registry);

		require(state.dimTextMat.id <= 15 && state.surgeMat.id <= 15 && state.duckMat.id <= 15 &&
					state.trackOnMat.id <= 15,
				"MusicScene material IDs must stay within the 16-slot GPU material limit.");
		require(services.render().getContextUI().charSpacing >= 9.5f,
				"MusicScene must keep default UI charSpacing so bitmap glyphs never fuse together.");
		static_assert(MusicSceneNamespace::State::NUM_DYNAMICS == 2,
					  "MusicScene must only keep SURGE and DUCK dynamic buttons.");

		for (size_t i = 0; i < MusicSceneNamespace::State::NUM_GAUGE_BARS; ++i)
		{
			require(registry.isEntityValid(state.surgeTrackBars[i]) && registry.isEntityValid(state.surgeFillBars[i]) &&
						registry.isEntityValid(state.duckTrackBars[i]) && registry.isEntityValid(state.duckFillBars[i]),
					"MusicScene 5-bar surge and duck equalizer entities must all be created.");

			const auto& surgeTrack = registry.getComponent<UIShape>(state.surgeTrackBars[i]);
			const auto& surgeFill = registry.getComponent<UIShape>(state.surgeFillBars[i]);
			const auto& duckTrack = registry.getComponent<UIShape>(state.duckTrackBars[i]);
			const auto& duckFill = registry.getComponent<UIShape>(state.duckFillBars[i]);
			require(surgeTrack.layer < surgeFill.layer && duckTrack.layer < duckFill.layer,
					"Surge and duck equalizer fills must draw in a higher SDF layer than their tracks.");
			require(surgeFill.parameters[DefaultShapes::Box::PosY] < 0.0f &&
						duckFill.parameters[DefaultShapes::Box::PosY] < 0.0f,
					"Idle equalizer fill bars must park off-screen.");
		}

		// Snapshot save & reload check
		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "MusicScene must save snapshots.");
		require(services.serialization().loadSnapshot(saved.path.string()), "MusicScene must restore snapshots.");

		// Verify TextButton/TextToggle attachment on right/left MusicScene labels
		require(registry.hasComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_SURGE]) &&
					registry.hasComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_DUCK]) &&
					registry.hasComponent<TextToggle>(state.trackLabels[0]),
				"MusicScene labels must attach TextButton and TextToggle engine components.");

		// Trigger SURGE via TextButton label and verify song auto-starts + background/bars activate
		registry.getComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_SURGE]).state =
			ButtonState::Down;
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		registry.getComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_SURGE]).state =
			ButtonState::Off;
		services.audio().music().update(0.25, 0.25);
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		MusicSceneNamespace::updateWorldVisualsSystem(registry, services);
		MusicSceneNamespace::statusTextSystem(registry, services);
		require(state.isPlaying && services.audio().music().getSurgeLevel() > 0.15f,
				"Clicking MusicScene SURGE TextButton must play and activate surge.");
		require(registry.getComponent<UIShape>(state.surgeFillBars[0]).parameters[DefaultShapes::Box::PosY] > 0.0f,
				"Active surge level must position equalizer fill bars visibly on screen.");
		require(registry.getComponent<Shape>(state.backgroundShape).parameters[2] > 5.2f,
				"Active surge must expand background grid visual energy.");
		require(registry.getComponent<UITextRenderer>(state.statusText).text == "SURGE ACTIVE",
				"Status HUD must reflect active surge state.");

		// Trigger DUCK via TextButton label and verify equalizer activates
		registry.getComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_DUCK]).state =
			ButtonState::Down;
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		registry.getComponent<TextButton>(state.dynamicLabels[MusicSceneNamespace::State::DYN_DUCK]).state =
			ButtonState::Off;
		services.audio().music().update(0.25, 0.50);
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		require(services.audio().music().getDuckingLevel() > 0.15f,
				"Clicking MusicScene DUCK TextButton must raise ducking level.");
		require(registry.getComponent<UIShape>(state.duckFillBars[0]).parameters[DefaultShapes::Box::PosY] > 0.0f,
				"Active duck level must position duck equalizer fill bars visibly on screen.");

		// Verify clearing engine dynamic effects parks equalizer fill bars cleanly off-screen
		services.audio().resetDynamicEffects();
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		require(registry.getComponent<UIShape>(state.surgeFillBars[0]).parameters[DefaultShapes::Box::PosY] < 0.0f &&
					registry.getComponent<UIShape>(state.duckFillBars[0]).parameters[DefaultShapes::Box::PosY] < 0.0f,
				"Zeroed surge/duck effects must park equalizer fill bars off-screen.");

		// Verify track toggle mutation propagates both from ShapeToggle pill and TextToggle label
		registry.getComponent<ShapeToggle>(state.trackToggles[0]).active = false;
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		require(!services.audio().music().getTrackToggles().lead &&
					!registry.getComponent<TextToggle>(state.trackLabels[0]).active,
				"Toggling LEAD pill off must mute Lead track and sync TextToggle.");
		registry.getComponent<TextToggle>(state.trackLabels[0]).active = true;
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		require(services.audio().music().getTrackToggles().lead &&
					registry.getComponent<ShapeToggle>(state.trackToggles[0]).active,
				"Toggling LEAD TextToggle back on must unmute Lead track and sync ShapeToggle.");

		// Verify responsive relayout on 640x480 viewport keeps both labels & gauges in bounds without text bleed
		WeirdRenderer::Display::width = 640;
		WeirdRenderer::Display::height = 480;
		MusicSceneNamespace::songLayoutSystem(registry, services);
		for (size_t i = 0; i < MusicSceneNamespace::State::NUM_DYNAMICS; ++i)
		{
			const auto& lblTf = registry.getComponent<Transform>(state.dynamicLabels[i]);
			require(lblTf.position.x > 500.0f && lblTf.position.x < 640.0f && lblTf.position.y > 60.0f &&
						lblTf.position.y < 430.0f,
					"MusicScene SURGE and DUCK labels must stay cleanly within 640x480 safe bounds.");
		}
		for (size_t i = 0; i < MusicSceneNamespace::State::NUM_GAUGE_BARS; ++i)
		{
			const auto& bar = registry.getComponent<UIShape>(state.surgeTrackBars[i]);
			require(bar.parameters[DefaultShapes::Box::PosX] > 420.0f &&
						bar.parameters[DefaultShapes::Box::PosX] < 635.0f,
					"MusicScene equalizer gauges must stay within 640x480 right panel bounds.");
		}
		services.audio().resetDynamicEffects();
		services.audio().setSong(nullptr, false);
		services.audio().music().setVolume(0.0f);
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
	}

	void testInstrumentScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-instrument-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<InstrumentScene>>(name);
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<InstrumentScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = InstrumentSceneNamespace::getState(registry);
		const auto initialRack = services.audio().getInstrumentRack();

		require(state.keys.size() == 24, "InstrumentScene must instantiate all 14 white + 10 black keys.");
		const auto& firstWhite = registry.getComponent<UIShape>(state.keys[0].entity);
		const auto& firstBlack = registry.getComponent<UIShape>(state.keys[14].entity);
		require(firstWhite.layer == 0 && firstBlack.layer == 1,
				"Black piano keys must use a higher SDF layer so they render cleanly above white keys.");

		const float whiteTop =
			firstWhite.parameters[DefaultShapes::Box::PosY] + firstWhite.parameters[DefaultShapes::Box::SizeY];
		const float blackTop =
			firstBlack.parameters[DefaultShapes::Box::PosY] + firstBlack.parameters[DefaultShapes::Box::SizeY];
		require(std::abs(blackTop - whiteTop) < 3.0f &&
					firstBlack.parameters[DefaultShapes::Box::PosY] > firstWhite.parameters[DefaultShapes::Box::PosY],
				"Black piano keys must anchor along the top edge of the white keys.");

		// Verify 2D world chassis cutout framing the piano + tight centered oscilloscope dot ribbon
		require(registry.isEntityValid(state.worldCutout) && registry.hasComponent<Shape>(state.worldCutout),
				"InstrumentScene must create a 2D world cutout framing the piano bay.");
		const auto& cut800 = registry.getComponent<Shape>(state.worldCutout);
		const float cutSizeX800 = cut800.parameters[DefaultShapes::Box::SizeX];
		const float cutSizeY800 = cut800.parameters[DefaultShapes::Box::SizeY];
		const float cutPosY800 = cut800.parameters[DefaultShapes::Box::PosY];
		const float camPosY800 = registry.getComponent<Transform>(services.render().getCameraEntity()).position.y;
		require(cut800.distanceFieldId == DefaultShapes::Box && cut800.combination == CombinationType::Subtraction &&
					cutSizeX800 > cutSizeY800 && cutSizeY800 > 1.0f && cutPosY800 < camPosY800,
				"Piano bay world cutout must be a wide landscape Subtraction RoundedBox aligned in Y-up space.");

		const float scopeFirstX = registry.getComponent<Transform>(state.scopeDots.front()).position.x;
		const float scopeSecondX = registry.getComponent<Transform>(state.scopeDots[1]).position.x;
		const float scopeLastX = registry.getComponent<Transform>(state.scopeDots.back()).position.x;
		const float scopeCenterX = 0.5f * (scopeFirstX + scopeLastX);
		require(std::abs(scopeCenterX - 400.0f) < 1.0f && (scopeSecondX - scopeFirstX) < 8.0f &&
					(scopeLastX - scopeFirstX) < 400.0f,
				"Oscilloscope dots must form a tight centered ribbon without wide gaps.");

		// Verify black key click takes priority when both white and black buttons report Down
		registry.getComponent<ShapeButton>(state.keys[0].entity).state = ButtonState::Down;
		registry.getComponent<ShapeButton>(state.keys[14].entity).state = ButtonState::Down;
		InstrumentSceneNamespace::inputHandlingSystem(registry, services);
		registry.getComponent<ShapeButton>(state.keys[0].entity).state = ButtonState::Off;
		registry.getComponent<ShapeButton>(state.keys[14].entity).state = ButtonState::Off;
		require(state.lastPlayedMidi == 61,
				"Clicking an overlapping black key (C#4=61) must not double-trigger underlying white key C4.");

		// Verify downward key press motion and glow dot activation
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		const auto& pressedBlack = registry.getComponent<UIShape>(state.keys[14].entity);
		require(pressedBlack.parameters[DefaultShapes::Box::PosY] < state.keys[14].basePosY,
				"Pressed piano keys must depress downward in screen Y-up space.");
		require(registry.getComponent<Transform>(state.keys[14].glowDot).position.y > 0.0f,
				"Active key press must reveal the key indicator glow dot.");

		// Verify 3-row UI hierarchy: Row 0 Top HUD buttons > Row 1 Sound & Timbre > Row 2 Chord/Arp/Drums
		const float row0Y =
			registry.getComponent<Transform>(state.actionLabels[InstrumentSceneNamespace::State::ACT_INST]).position.y;
		const float row1Y =
			registry.getComponent<Transform>(state.actionLabels[InstrumentSceneNamespace::State::ACT_TONE]).position.y;
		const float row2Y =
			registry.getComponent<Transform>(state.actionLabels[InstrumentSceneNamespace::State::ACT_CHORD]).position.y;
		require(row0Y > 700.0f && row0Y > row1Y && row1Y > row2Y,
				"Native UI must place Top HUD buttons above Row 1 (Sound/Timbre) and Row 2 (Chord/Arp/Drums).");

		// Verify dynamic Sound & Timbre controls based on active instrument and waveform
		require(state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_TIMBRE] == "SIN" &&
					state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_MOD] == "-",
				"Lead SoftSine must show SIN and disabled - in Sound & Timbre slots.");

		auto clickActionLabel = [&](InstrumentSceneNamespace::State::ActionBtn btn)
		{
			registry.getComponent<TextButton>(state.actionLabels[btn]).state = ButtonState::Down;
			InstrumentSceneNamespace::inputHandlingSystem(registry, services);
			registry.getComponent<TextButton>(state.actionLabels[btn]).state = ButtonState::Off;
		};

		clickActionLabel(InstrumentSceneNamespace::State::ACT_TIMBRE);
		clickActionLabel(InstrumentSceneNamespace::State::ACT_TIMBRE);
		clickActionLabel(InstrumentSceneNamespace::State::ACT_TIMBRE);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_TIMBRE] == "FM" &&
					state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_MOD].rfind("FM:", 0) == 0,
				"Switching to FMPluck waveform must dynamically switch MOD slot to FM index.");

		state.selectedInstrument = WeirdEngine::WeirdAudio::Instrument::Kick;
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_TIMBRE] == "808" &&
					state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_MOD] == "H:KK",
				"Switching Active Instrument to Kick must dynamically switch Row 1 to Drum Kit & HIT trigger.");
		state.selectedInstrument = WeirdEngine::WeirdAudio::Instrument::Lead;
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);

		// Verify top HUD TextButtons (ACT_OCT, ACT_ROOT, ACT_SCALE) and bottom TextToggle (ARP)
		const int prevOct = state.currentOctave;
		clickActionLabel(InstrumentSceneNamespace::State::ACT_OCT);
		require(state.currentOctave == prevOct + 1, "Clicking top HUD octave text button must advance octave.");

		const int prevRoot = state.highlightRootNote;
		clickActionLabel(InstrumentSceneNamespace::State::ACT_ROOT);
		require(state.highlightRootNote == (prevRoot + 1) % 12,
				"Clicking top HUD root key text button must cycle scale root key.");

		const int prevScale = state.highlightScaleIdx;
		clickActionLabel(InstrumentSceneNamespace::State::ACT_SCALE);
		require(state.highlightScaleIdx == (prevScale + 1) % 8,
				"Clicking top HUD scale text button must cycle active scale.");

		require(registry.hasComponent<TextToggle>(state.arpToggleLabel),
				"InstrumentScene ARP label must attach TextToggle.");
		registry.getComponent<TextToggle>(state.arpToggleLabel).active = true;
		InstrumentSceneNamespace::inputHandlingSystem(registry, services);
		require(state.arpEnabled, "ARP TextToggle label must enable arpeggiator.");

		// Verify responsive 640x480 resize reflow + font-safe note HUD + non-overlapping text label bounds
		WeirdRenderer::Display::width = 640;
		WeirdRenderer::Display::height = 480;
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.lastWidth == 640 && state.lastHeight == 480,
				"InstrumentScene must detect viewport resize and reflow layout.");
		const auto& cut640 = registry.getComponent<Shape>(state.worldCutout);
		require(std::abs(cut640.parameters[DefaultShapes::Box::SizeX] - cutSizeX800) > 0.1f,
				"Piano bay world cutout must recompute world bounds cleanly on viewport resize.");
		require(state.cachedHudNote.find('#') == std::string::npos,
				"On-screen note HUD must avoid '#' characters missing from the bitmap font.");

		auto checkNoHorizontalOverlap = [&](std::initializer_list<Entity> entities, const char* msg)
		{
			std::vector<Entity> list(entities);
			for (size_t i = 0; i + 1 < list.size(); ++i)
			{
				float x0 = registry.getComponent<Transform>(list[i]).position.x;
				float x1 = registry.getComponent<Transform>(list[i + 1]).position.x;
				float halfW0 = 0.5f * registry.getComponent<TextRenderer>(list[i]).text.size() * 30.0f;
				float halfW1 = 0.5f * registry.getComponent<TextRenderer>(list[i + 1]).text.size() * 30.0f;
				require((x1 - halfW1) - (x0 + halfW0) > 4.0f, msg);
			}
		};

		checkNoHorizontalOverlap({state.actionLabels[InstrumentSceneNamespace::State::ACT_INST],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_TIMBRE],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_OCT]},
								 "Top-left HUD text buttons must not overlap at 640x480.");
		checkNoHorizontalOverlap({state.actionLabels[InstrumentSceneNamespace::State::ACT_ROOT],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_SCALE], state.hudNoteText},
								 "Top-right HUD text buttons and note readout must not overlap at 640x480.");
		checkNoHorizontalOverlap({state.actionLabels[InstrumentSceneNamespace::State::ACT_MOD],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_TONE],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_FLT]},
								 "Bottom Row 1 text buttons must not overlap at 640x480.");
		checkNoHorizontalOverlap({state.actionLabels[InstrumentSceneNamespace::State::ACT_CHORD], state.arpToggleLabel,
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_KICK],
								  state.actionLabels[InstrumentSceneNamespace::State::ACT_SNARE]},
								 "Bottom Row 2 text buttons must not overlap at 640x480.");

		for (size_t i = InstrumentSceneNamespace::State::ACT_MOD; i < InstrumentSceneNamespace::State::NUM_ACTION_BTNS;
			 ++i)
		{
			const auto& pos = registry.getComponent<Transform>(state.actionLabels[i]).position;
			require(pos.x > 0.0f && pos.x < 640.0f && pos.y > 20.0f && pos.y < 140.0f,
					"InstrumentScene bottom UI labels must fit cleanly below keyboard at 640x480.");
		}

		// Verify wide desktop window (1600x900) clamps control container width cleanly under the piano
		WeirdRenderer::Display::width = 1600;
		WeirdRenderer::Display::height = 900;
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		const float chordX1600 =
			registry.getComponent<Transform>(state.actionLabels[InstrumentSceneNamespace::State::ACT_CHORD]).position.x;
		const float snrX1600 =
			registry.getComponent<Transform>(state.actionLabels[InstrumentSceneNamespace::State::ACT_SNARE]).position.x;
		require(chordX1600 > 430.0f && (snrX1600 - chordX1600) < 620.0f,
				"InstrumentScene bottom control container must stay clamped beneath the piano on wide windows.");

		// Verify joystick radial deadzone, same-octant hysteresis hold, and opposite-side snapback rejection
		require(InstrumentSceneNamespace::getStickOctant(0.0f, -0.50f, -1, state.stickDeadzone) == -1,
				"Stick magnitude inside radial deadzone must remain neutral.");
		require(InstrumentSceneNamespace::getStickOctant(0.0f, -0.80f, -1, state.stickDeadzone) == 0,
				"Stick pushed past deadzone straight Up must select octant 0.");
		require(InstrumentSceneNamespace::getStickOctant(0.0f, -0.54f, 0, state.stickDeadzone) == 0,
				"Held octant inside release hysteresis band must not flicker off.");
		require(InstrumentSceneNamespace::getStickOctant(0.0f, 0.54f, 0, state.stickDeadzone) == -1,
				"Opposite-direction snapback inside deadzone must return neutral instead of firing a ghost note.");

		// Verify Left Stick scale-filtered selection, L1 black keys, R1 right-side octave, and dot above key
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		state.currentOctave = 4;
		state.highlightScaleIdx = 2; // C Major (C, D, E, F, G, A, B)
		state.highlightRootNote = 0;
		state.lastPlayedMidi = -1;
		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, 0.0f, false, false);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == -1 &&
					registry.getComponent<UIShape>(state.selectionDot).parameters[DefaultShapes::Circle::Radius] ==
						0.0f,
				"Selection dot must be hidden when no stick note is selected.");

		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, -0.85f, false, false);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == 60 && state.lastPlayedMidi == -1,
				"Moving left stick Up must select MIDI 60 without immediately playing it.");
		{
			const auto& dot = registry.getComponent<UIShape>(state.selectionDot);
			const auto& key0 = state.keys[0];
			require(dot.parameters[DefaultShapes::Circle::Radius] > 0.0f &&
						std::abs(dot.parameters[DefaultShapes::Circle::PosX] - key0.basePosX) < 1.0f &&
						dot.parameters[DefaultShapes::Circle::PosY] > key0.basePosY + key0.sizeY,
					"Selection dot must appear above white key C4.");
		}

		// C Major has no black keys, so L1 black-key mode must clear selection and hide the dot
		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, -0.85f, true, false);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == -1 &&
					registry.getComponent<UIShape>(state.selectionDot).parameters[DefaultShapes::Circle::Radius] ==
						0.0f,
				"Black-key selection in C Major must yield no note and hide the dot.");

		// G Major (F#) has 1 black key (MIDI 66 = F#4, keys[16])
		state.highlightRootNote = 7;
		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, -0.85f, true, false);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == 66, "G Major black-key selection must select F#4 (MIDI 66).");
		{
			const auto& dot = registry.getComponent<UIShape>(state.selectionDot);
			const auto& key16 = state.keys[16];
			require(dot.parameters[DefaultShapes::Circle::Radius] > 0.0f &&
						std::abs(dot.parameters[DefaultShapes::Circle::PosX] - key16.basePosX) < 1.0f &&
						dot.parameters[DefaultShapes::Circle::PosY] > key16.basePosY + key16.sizeY,
					"Selection dot must appear above black key F#4.");
		}

		// R1 shifts to right side of piano (+12 semitones -> F#5 = 78)
		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, -0.85f, true, true);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == 78, "R1 modifier must shift selection to right-side octave (MIDI 78).");

		InstrumentSceneNamespace::updateGamepadStickSelection(state, 0.0f, 0.0f, false, false);
		InstrumentSceneNamespace::updateVisualsSystem(registry, services);
		require(state.selectedStickMidi == -1 &&
					registry.getComponent<UIShape>(state.selectionDot).parameters[DefaultShapes::Circle::Radius] ==
						0.0f,
				"Releasing left stick must clear selection and hide the selection dot.");

		services.audio().setInstrumentRack(initialRack);
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
	}

	void testMenuScene(bool threaded)
	{
		auto& manager = configureManager(threaded);
		const std::string name = std::string("test-menu-") + (threaded ? "threaded" : "single");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<MenuScene>>(name);
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<MenuScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = MenuSceneNamespace::getState(registry);

		for (size_t i = 0; i < MenuSceneNamespace::State::CATEGORY_COUNT; ++i)
		{
			require(registry.isEntityValid(state.categoryIcons[i]) &&
						registry.hasComponent<ShapeButton>(state.categoryIcons[i]),
					"MenuScene category icons must attach ShapeButton components.");
		}
		for (int slot = 0; slot < MenuSceneNamespace::State::VISIBLE_ITEM_SLOTS; ++slot)
		{
			require(registry.isEntityValid(state.itemLabels[slot]) &&
						registry.hasComponent<TextButton>(state.itemLabels[slot]),
					"MenuScene item label slots must attach TextButton components.");
		}

		const auto& uiWhite = services.materials2D().get("ui_white");
		const auto& uiDimmed = services.materials2D().get("ui_dimmed");
		const auto& uiAccent = services.materials2D().get("ui_accent");

		require(state.selectedCategory == 0 && state.selectedItem == 0 &&
					registry.getComponent<UITextRenderer>(state.itemLabels[4]).text == "TETROMINOES" &&
					registry.getComponent<UITextRenderer>(state.itemLabels[5]).text == "SHAPES" &&
					registry.getComponent<UITextRenderer>(state.itemLabels[5]).material == uiDimmed.id,
				"MenuScene must populate aligned initial item slots on startup.");

		// Snapshot save & reload check
		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "MenuScene must save snapshots.");
		require(services.serialization().loadSnapshot(saved.path.string()), "MenuScene must restore snapshots.");

		// Verify hovering an item slot TextButton highlights with ui_accent even across menuAnimationSystem updates
		SDFRenderSystemContext uiCtx{};
		const auto& slot5Tf = registry.getComponent<Transform>(state.itemLabels[5]);
		Input::SetMousePosition(slot5Tf.position.x + 25.0f, 800.0f - slot5Tf.position.y);
		ButtonSystem::updateTextButtons(registry, uiCtx);
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		require(registry.getComponent<TextButton>(state.itemLabels[5]).hovered &&
					registry.getComponent<UITextRenderer>(state.itemLabels[5]).material == uiAccent.id,
				"Hovering MenuScene item label must preserve accent hover material.");

		Input::SetMousePosition(0.0f, 0.0f);
		ButtonSystem::updateTextButtons(registry, uiCtx);
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		require(!registry.getComponent<TextButton>(state.itemLabels[5]).hovered &&
					registry.getComponent<UITextRenderer>(state.itemLabels[5]).material == uiDimmed.id,
				"Leaving MenuScene item label must restore base dimmed material.");

		// Regression: clicking an item and holding mouse while list animates must not reset slots into a second click
		Input::clearUIClick();
		Input::SetMousePosition(slot5Tf.position.x + 25.0f, 800.0f - slot5Tf.position.y);
		ButtonSystem::updateTextButtons(registry, uiCtx, true, true);
		MenuSceneNamespace::menuInputSystem(registry, services);
		require(state.selectedItem == 1 &&
					registry.getComponent<TextButton>(state.itemLabels[5]).state == ButtonState::Off,
				"Clicking MenuScene item must select it and clear recycled slot button states.");
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		Input::clearUIClick();
		ButtonSystem::updateTextButtons(registry, uiCtx, true, false);
		MenuSceneNamespace::menuInputSystem(registry, services);
		require(state.selectedItem == 1,
				"Continuing to hold mouse while MenuScene items animate must not click another item.");
		Input::clearUIClick();
		Input::SetMousePosition(0.0f, 0.0f);
		ButtonSystem::updateTextButtons(registry, uiCtx, false, false);
		state.selectedItem = 0;
		state.smoothItem = 0.0f;
		MenuSceneNamespace::menuAnimationSystem(registry, services);

		// Click unselected item slot 6 ("ROPE") via TextButton -> selects item 2
		registry.getComponent<TextButton>(state.itemLabels[6]).state = ButtonState::Down;
		MenuSceneNamespace::menuInputSystem(registry, services);
		registry.getComponent<TextButton>(state.itemLabels[6]).state = ButtonState::Off;
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		require(state.selectedItem == 2 && registry.getComponent<UITextRenderer>(state.itemLabels[4]).text == "ROPE" &&
					registry.getComponent<UITextRenderer>(state.itemLabels[4]).material == uiWhite.id,
				"Clicking MenuScene item TextButton must select that item and place it in active slot 4.");

		// Verify ShapeButton hover pulse & category switch on SETTINGS icon (index 2)
		const float baseIconScale = registry.getComponent<UIShape>(state.categoryIcons[2]).parameters[2];
		registry.getComponent<ShapeButton>(state.categoryIcons[2]).hovered = true;
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		require(registry.getComponent<UIShape>(state.categoryIcons[2]).parameters[2] > baseIconScale + 2.0f,
				"Hovering MenuScene category ShapeButton must enlarge the category icon.");
		registry.getComponent<ShapeButton>(state.categoryIcons[2]).hovered = false;

		registry.getComponent<ShapeButton>(state.categoryIcons[2]).state = ButtonState::Down;
		MenuSceneNamespace::menuInputSystem(registry, services);
		registry.getComponent<ShapeButton>(state.categoryIcons[2]).state = ButtonState::Off;
		MenuSceneNamespace::menuAnimationSystem(registry, services);
		require(state.selectedCategory == 2 &&
					registry.getComponent<UITextRenderer>(state.itemLabels[4]).text == "THEME",
				"Clicking MenuScene category ShapeButton must switch to SETTINGS category.");

		// Click active THEME TextButton (slot 4) -> cycles theme without keyboard/manual box math
		const int prevTheme = state.currentTheme;
		registry.getComponent<TextButton>(state.itemLabels[4]).state = ButtonState::Down;
		MenuSceneNamespace::menuInputSystem(registry, services);
		registry.getComponent<TextButton>(state.itemLabels[4]).state = ButtonState::Off;
		require(state.currentTheme != prevTheme,
				"Clicking active THEME MenuScene TextButton must trigger confirm and cycle theme.");

		// Restore default menu category & item so subsequent single/threaded passes start cleanly
		registry.getComponent<ShapeButton>(state.categoryIcons[0]).state = ButtonState::Down;
		MenuSceneNamespace::menuInputSystem(registry, services);
		registry.getComponent<ShapeButton>(state.categoryIcons[0]).state = ButtonState::Off;
		state.selectedItem = 0;
		MenuSceneNamespace::menuInputSystem(registry, services);
		Input::suppressMouseInput();
		Input::SetMousePosition(0.0f, 0.0f);
		services.audio().resetDynamicEffects();
		services.audio().setSong(nullptr, false);
		services.audio().music().setVolume(0.0f);
	}
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(argc, argv, "WeirdSampleSceneTests",
								{
									{"RopeScene-single", []() { testRopeScene(false); }},
									{"RopeScene-threaded", []() { testRopeScene(true); }},
									{"TetrominoScene-single", []() { testTetrominoScene(false); }},
									{"TetrominoScene-threaded", []() { testTetrominoScene(true); }},
									{"ThrustScene-single", []() { testThrustScene(false); }},
									{"ThrustScene-threaded", []() { testThrustScene(true); }},
									{"MusicScene-single", []() { testMusicScene(false); }},
									{"MusicScene-threaded", []() { testMusicScene(true); }},
									{"InstrumentScene-single", []() { testInstrumentScene(false); }},
									{"InstrumentScene-threaded", []() { testInstrumentScene(true); }},
									{"MenuScene-single", []() { testMenuScene(false); }},
									{"MenuScene-threaded", []() { testMenuScene(true); }},
								});
}
