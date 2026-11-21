#include "ImageScene.h"
#include "InstrumentScene.h"
#include "MenuScene.h"
#include "MusicScene.h"
#include "RopeScene.h"
#include "TetrominoScene.h"
#include "ThrustScene.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/services/ShapeService.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/shapes/NodeGraphSerializer.h"
#include "weird-engine/systems/ButtonSystem.h"
#include "weird-engine/systems/PhysicsSystem2D.h"
#include "weird-engine/systems/Shape2DPacking.h"
#include "weird-physics/components/RigidBodyManager.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "weird-physics/components/Spring.h"

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

using namespace WeirdEngine;

namespace First
{
	struct Value
	{
		int number = 0;
	};
	struct State
	{
		int number = 0;
	};
} // namespace First

namespace Second
{
	struct Value
	{
		int number = 0;
	};
	struct State
	{
		int number = 0;
	};
} // namespace Second

namespace
{
	struct OwningState
	{
		std::string name;
		std::vector<int> values;
	};

	static_assert(!is_snapshot_serializable_v<OwningState>);
	static_assert(!is_snapshot_serializable_v<std::mutex>);
	static_assert(!is_snapshot_serializable_v<int*>);
	static_assert(!is_snapshot_serializable_v<ImageSceneNamespace::State>);
	static_assert(is_snapshot_serializable_v<Transform>);
	static_assert(is_snapshot_serializable_v<RigidBody2D>);
	static_assert(is_snapshot_serializable_v<ECS::Camera>);

	void require(bool condition, const char* message)
	{
		if (!condition)
			throw std::runtime_error(message);
	}

	struct SnapshotFile
	{
		std::filesystem::path path;

		SnapshotFile()
		{
			static size_t sequence = 0;
			const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
			path = std::filesystem::temp_directory_path() /
				   ("weird-snapshot-test-" + std::to_string(tick) + "-" + std::to_string(sequence++) + ".weirdsnap");
		}

		~SnapshotFile()
		{
			std::error_code error;
			std::filesystem::remove(path, error);
		}
	};

	template <typename Base> class InspectableScene : public Base
	{
	public:
		Registry* registry = nullptr;
		ServiceProvider* services = nullptr;

	protected:
		void onStart(Registry& sceneRegistry, ServiceProvider& sceneServices) override
		{
			registry = &sceneRegistry;
			services = &sceneServices;
		}
	};

	void testUnsupportedTypes()
	{
		Registry supported;
		const Entity entity = supported.createEntity();
		supported.addComponent<First::Value>(entity).number = 7;
		std::stringstream saved;
		require(supported.serializeSnapshot(saved), "Value components must support snapshots.");

		Registry unsupported;
		const Entity existing = unsupported.createEntity();
		unsupported.addComponent<First::Value>(existing).number = 19;
		unsupported.emplaceState<ImageSceneNamespace::State>();
		std::stringstream rejected;
		require(!unsupported.serializeSnapshot(rejected), "Image state must not be written as raw bytes.");
		require(rejected.str().empty(), "Unsupported snapshots must not write partial data.");
		require(!unsupported.deserializeSnapshot(saved), "Unsupported state must reject snapshot restoration.");
		require(unsupported.getComponent<First::Value>(existing).number == 19,
				"Unsupported loads must retain live data.");

		Registry components;
		components.addComponent<OwningState>(components.createEntity());
		require(!components.canSerializeSnapshot(), "Owning components must reject raw snapshots.");
	}

	void testTypeIdentity()
	{
		Registry source;
		const Entity entity = source.createEntity();
		source.addComponent<First::Value>(entity).number = 11;
		source.addComponent<Second::Value>(entity).number = 22;
		source.emplaceState<First::State>().number = 111;
		source.emplaceState<Second::State>().number = 222;
		std::stringstream saved;
		require(source.serializeSnapshot(saved), "The namespace test must save.");

		Registry restored;
		restored.registerComponent<Second::Value>();
		restored.registerComponent<First::Value>();
		restored.emplaceState<Second::State>();
		restored.emplaceState<First::State>();
		require(restored.deserializeSnapshot(saved), "The namespace test must load.");
		require(restored.getComponent<First::Value>(entity).number == 11, "First::Value must remain distinct.");
		require(restored.getComponent<Second::Value>(entity).number == 22, "Second::Value must remain distinct.");
		require(restored.getState<First::State>()->number == 111, "First::State must remain distinct.");
		require(restored.getState<Second::State>()->number == 222, "Second::State must remain distinct.");
	}

	void testDeferredDestruction()
	{
		Registry registry;
		const Entity removed = registry.createEntity();
		const Entity retained = registry.createEntity();
		registry.addComponent<First::Value>(removed).number = 1;
		registry.addComponent<First::Value>(retained).number = 2;
		registry.destroyEntity(removed);
		registry.destroyEntity(removed);
		registry.destroyEntity(Entity{99});
		std::stringstream saved;
		require(registry.serializeSnapshot(saved), "Pending deletions must permit saving.");
		require(!registry.hasComponent<First::Value>(removed), "Saving must finish deferred deletions.");
		require(registry.deserializeSnapshot(saved), "The deletion test must load.");
		require(!registry.hasComponent<First::Value>(removed), "Deleted components must not return.");
		require(registry.getComponent<First::Value>(retained).number == 2, "Other components must survive compaction.");
		registry.destroyEntity(removed);
		registry.freeRemovedComponents();
		const Entity reused = registry.createEntity();
		const Entity next = registry.createEntity();
		require(reused == removed && next != reused, "Each deleted ID must be released only once.");
		registry.addComponent<First::Value>(reused);
		registry.destroyEntity(reused);
		registry.destroyEntity(reused);
		registry.freeRemovedComponents();
		require(registry.createEntity() == reused, "A reused entity must support later destruction.");
		require(registry.createEntity() != reused, "Later destruction must also release the ID only once.");
	}

	void testDamagedSnapshots()
	{
		// 1. Truncated snapshot tests
		{
			Registry source;
			const Entity e = source.createEntity();
			source.addComponent<First::Value>(e).number = 42;
			source.emplaceState<First::State>().number = 100;
			std::stringstream fullSaved;
			require(source.serializeSnapshot(fullSaved), "Valid snapshot serialization must succeed.");
			const std::string validBytes = fullSaved.str();

			const size_t lengths[] = {0, 4, 8, 16, 24, validBytes.size() / 2, validBytes.size() - 1};
			for (size_t len : lengths)
			{
				Registry target;
				const Entity liveEnt = target.createEntity();
				target.addComponent<First::Value>(liveEnt).number = 999;

				std::stringstream truncated(validBytes.substr(0, len));
				require(!target.deserializeSnapshot(truncated), "Truncated snapshot must fail to load.");
				require(target.getComponent<First::Value>(liveEnt).number == 999,
						"Failed snapshot load must not wipe live scene.");
			}
		}

		// 2. Out-of-bounds entityCount (> MAX_ENTITIES)
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = MAX_ENTITIES.id + 50;
			uint32_t freeCount = 0;
			uint32_t managerCount = 0;
			uint32_t stateCount = 0;
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&stateCount), 4);

			Registry reg;
			const Entity live = reg.createEntity();
			reg.addComponent<First::Value>(live).number = 77;
			require(!reg.deserializeSnapshot(bad), "Oversized entityCount must reject load.");
			require(reg.getComponent<First::Value>(live).number == 77, "Scene must be preserved on reject.");
		}

		// 3. Out-of-bounds free entity (> entityCount or >= MAX_ENTITIES)
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 5;
			uint32_t freeCount = 1;
			uint32_t fe = 100;
			uint32_t managerCount = 0;
			uint32_t stateCount = 0;
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&fe), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&stateCount), 4);

			Registry reg;
			const Entity live = reg.createEntity();
			reg.addComponent<First::Value>(live).number = 88;
			require(!reg.deserializeSnapshot(bad), "Oversized free entity must reject load without throwing.");
			require(reg.getComponent<First::Value>(live).number == 88, "Scene must be preserved on reject.");
		}

		// 4. Duplicate free entity ID
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 5;
			uint32_t freeCount = 2;
			uint32_t fe1 = 2;
			uint32_t fe2 = 2;
			uint32_t managerCount = 0;
			uint32_t stateCount = 0;
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&fe1), 4);
			bad.write(reinterpret_cast<char*>(&fe2), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&stateCount), 4);

			Registry reg;
			require(!reg.deserializeSnapshot(bad), "Duplicate free entity ID must reject load.");
		}

		// 5. Capped nameLen (> 1024)
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 1;
			uint32_t freeCount = 0;
			uint32_t managerCount = 1;
			uint32_t nameLen = 1000000;
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&nameLen), 4);

			Registry reg;
			require(!reg.deserializeSnapshot(bad), "Excessive name length must reject load immediately.");
		}

		// 6. Out-of-bounds entityId in component data
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 2;
			uint32_t freeCount = 0;
			uint32_t managerCount = 1;
			std::string compName = "First::Value";
			uint32_t nameLen = static_cast<uint32_t>(compName.size());
			uint32_t count = 1;
			uint32_t elemSize = sizeof(First::Value);
			uint32_t badEntityId = MAX_ENTITIES.id + 10;
			First::Value val{123};

			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&nameLen), 4);
			bad.write(compName.data(), nameLen);
			bad.write(reinterpret_cast<char*>(&count), 4);
			bad.write(reinterpret_cast<char*>(&elemSize), 4);
			bad.write(reinterpret_cast<char*>(&badEntityId), 4);
			bad.write(reinterpret_cast<char*>(&val), sizeof(val));
			uint32_t stateCount = 0;
			bad.write(reinterpret_cast<char*>(&stateCount), 4);

			Registry reg;
			reg.registerComponent<First::Value>();
			const Entity live = reg.createEntity();
			reg.addComponent<First::Value>(live).number = 55;
			require(!reg.deserializeSnapshot(bad), "Out-of-bounds entityId in component must reject load.");
			require(reg.getComponent<First::Value>(live).number == 55, "Live data must be preserved on reject.");
		}

		// 7. Component size mismatch rejected without half-wiped registry
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 1;
			uint32_t freeCount = 0;
			uint32_t managerCount = 1;
			std::string compName = "First::Value";
			uint32_t nameLen = static_cast<uint32_t>(compName.size());
			uint32_t count = 1;
			uint32_t wrongElemSize = sizeof(First::Value) + 16;
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&nameLen), 4);
			bad.write(compName.data(), nameLen);
			bad.write(reinterpret_cast<char*>(&count), 4);
			bad.write(reinterpret_cast<char*>(&wrongElemSize), 4);
			std::vector<char> dummyPayload(count * (sizeof(uint32_t) + wrongElemSize), 0);
			bad.write(dummyPayload.data(), dummyPayload.size());
			uint32_t stateCount = 0;
			bad.write(reinterpret_cast<char*>(&stateCount), 4);

			Registry reg;
			reg.registerComponent<First::Value>();
			const Entity live = reg.createEntity();
			reg.addComponent<First::Value>(live).number = 1234;
			require(!reg.deserializeSnapshot(bad), "Size mismatch must report false from deserializeSnapshot.");
			require(reg.getComponent<First::Value>(live).number == 1234,
					"Size mismatch must retain live data and not leave scene half-wiped.");
		}

		// 8. 32-bit arithmetic overflow in skip path
		{
			std::stringstream bad;
			uint32_t magic = Registry::SNAPSHOT_MAGIC;
			uint32_t version = Registry::SNAPSHOT_VERSION;
			double simTime = 0.0;
			uint32_t entityCount = 1;
			uint32_t freeCount = 0;
			uint32_t managerCount = 1;
			std::string compName = "Unregistered::Component";
			uint32_t nameLen = static_cast<uint32_t>(compName.size());
			uint32_t count = 100000;
			uint32_t elemSize = 50000; // count * (4 + elemSize) overflows 32-bit uint
			bad.write(reinterpret_cast<char*>(&magic), 4);
			bad.write(reinterpret_cast<char*>(&version), 4);
			bad.write(reinterpret_cast<char*>(&simTime), 8);
			bad.write(reinterpret_cast<char*>(&entityCount), 4);
			bad.write(reinterpret_cast<char*>(&freeCount), 4);
			bad.write(reinterpret_cast<char*>(&managerCount), 4);
			bad.write(reinterpret_cast<char*>(&nameLen), 4);
			bad.write(compName.data(), nameLen);
			bad.write(reinterpret_cast<char*>(&count), 4);
			bad.write(reinterpret_cast<char*>(&elemSize), 4);

			Registry reg;
			require(!reg.deserializeSnapshot(bad), "Overflowing skip path on truncated stream must reject load.");
		}
	}

	void testDeterministicSnapshots()
	{
		std::stringstream stream1;
		{
			Registry reg1;
			const Entity e = reg1.createEntity();
			auto& rb = reg1.addComponent<RigidBody2D>(e);
			rb.mass = 5.0f;
			rb.velocity = glm::vec2(1.0f, -2.0f);
			rb.isFixed = false;
			rb.enableCollision = true;
			rb.enableShapeCollision = false;

			auto& sp = reg1.addComponent<Spring>(e);
			sp.entityA = e;
			sp.stiffness = 2.5f;
			sp.restDistance = 3.0f;

			require(reg1.serializeSnapshot(stream1), "Registry 1 snapshot must save.");
		}

		std::stringstream stream2;
		{
			Registry reg2;
			const Entity e = reg2.createEntity();
			alignas(RigidBody2D) char dirtyBytes[sizeof(RigidBody2D)];
			std::memset(dirtyBytes, 0xAA, sizeof(RigidBody2D));
			auto* dirtyRb = reinterpret_cast<RigidBody2D*>(dirtyBytes);
			dirtyRb->simulationId = INVALID_SIMULATION_ID;
			dirtyRb->mass = 5.0f;
			dirtyRb->velocity = glm::vec2(1.0f, -2.0f);
			dirtyRb->pendingImpulseForce = glm::vec2(0.0f, 0.0f);
			dirtyRb->pendingContinuousForce = glm::vec2(0.0f, 0.0f);
			dirtyRb->type = BodyType::Dynamic;
			dirtyRb->isFixed = false;
			dirtyRb->enableCollision = true;
			dirtyRb->enableShapeCollision = false;

			reg2.addComponent<RigidBody2D>(e) = *dirtyRb;

			alignas(Spring) char dirtySpring[sizeof(Spring)];
			std::memset(dirtySpring, 0xBB, sizeof(Spring));
			auto* spDirty = reinterpret_cast<Spring*>(dirtySpring);
			spDirty->entityA = e;
			spDirty->entityB = INVALID_ENTITY;
			spDirty->stiffness = 2.5f;
			spDirty->restDistance = 3.0f;

			reg2.addComponent<Spring>(e) = *spDirty;

			require(reg2.serializeSnapshot(stream2), "Registry 2 snapshot must save.");
		}

		require(stream1.str().size() == stream2.str().size(), "Snapshot sizes must match.");
		require(stream1.str() == stream2.str(), "Snapshots of identical state must match byte-for-byte.");
	}

	void testRopeScene(SceneManager& manager, const std::string& name)
	{
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

	void testTetrominoScene(SceneManager& manager, const std::string& name)
	{
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

	void testThrustScene(SceneManager& manager, const std::string& name)
	{
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

		// Verify Fuel UI groups are separated (active fill draws over track) and label clears the bar vertically.
		require(registry.isEntityValid(state.fuelBgLineEntity) && registry.isEntityValid(state.fuelLineEntity) &&
					registry.isEntityValid(state.fuelLabelEntity),
				"ThrustScene fuel UI entities must all be created.");
		const auto& bgUi = registry.getComponent<UIShape>(state.fuelBgLineEntity);
		const auto& fuelUi = registry.getComponent<UIShape>(state.fuelLineEntity);
		const auto& labelTransform = registry.getComponent<Transform>(state.fuelLabelEntity);
		const float trackTop =
			bgUi.parameters[DefaultShapes::Line::StartY] + bgUi.parameters[DefaultShapes::Line::Width];
		const float labelBottom = labelTransform.position.y - 40.0f;
		require(bgUi.groupIdx < fuelUi.groupIdx,
				"Fuel bar fill must use a higher SDF group than its track so it is not masked.");
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

	void testMusicScene(SceneManager& manager, const std::string& name)
	{
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
			require(surgeTrack.groupIdx < surgeFill.groupIdx && duckTrack.groupIdx < duckFill.groupIdx,
					"Surge and duck equalizer fills must draw in a higher SDF group than their tracks.");
			require(surgeFill.parameters[DefaultShapes::RoundBox::PosY] < 0.0f &&
						duckFill.parameters[DefaultShapes::RoundBox::PosY] < 0.0f,
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
		require(registry.getComponent<UIShape>(state.surgeFillBars[0]).parameters[DefaultShapes::RoundBox::PosY] > 0.0f,
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
		require(registry.getComponent<UIShape>(state.duckFillBars[0]).parameters[DefaultShapes::RoundBox::PosY] > 0.0f,
				"Active duck level must position duck equalizer fill bars visibly on screen.");

		// Verify clearing engine dynamic effects parks equalizer fill bars cleanly off-screen
		services.audio().resetDynamicEffects();
		MusicSceneNamespace::dynamicControlsSystem(registry, services);
		require(
			registry.getComponent<UIShape>(state.surgeFillBars[0]).parameters[DefaultShapes::RoundBox::PosY] < 0.0f &&
				registry.getComponent<UIShape>(state.duckFillBars[0]).parameters[DefaultShapes::RoundBox::PosY] < 0.0f,
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
			require(bar.parameters[DefaultShapes::RoundBox::PosX] > 420.0f &&
						bar.parameters[DefaultShapes::RoundBox::PosX] < 635.0f,
					"MusicScene equalizer gauges must stay within 640x480 right panel bounds.");
		}
		services.audio().resetDynamicEffects();
		services.audio().setSong(nullptr, false);
		services.audio().music().setVolume(0.0f);
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
	}

	void testInstrumentScene(SceneManager& manager, const std::string& name)
	{
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<InstrumentScene>>(name);
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<InstrumentScene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;
		auto& state = InstrumentSceneNamespace::getState(registry);

		require(state.keys.size() == 24, "InstrumentScene must instantiate all 14 white + 10 black keys.");
		const auto& firstWhite = registry.getComponent<UIShape>(state.keys[0].entity);
		const auto& firstBlack = registry.getComponent<UIShape>(state.keys[14].entity);
		require(firstWhite.groupIdx == 0 && firstBlack.groupIdx == 1,
				"Black piano keys must use a higher SDF group so they render cleanly above white keys.");

		const float whiteTop = firstWhite.parameters[DefaultShapes::RoundBox::PosY] +
							   firstWhite.parameters[DefaultShapes::RoundBox::SizeY];
		const float blackTop = firstBlack.parameters[DefaultShapes::RoundBox::PosY] +
							   firstBlack.parameters[DefaultShapes::RoundBox::SizeY];
		require(std::abs(blackTop - whiteTop) < 3.0f && firstBlack.parameters[DefaultShapes::RoundBox::PosY] >
															firstWhite.parameters[DefaultShapes::RoundBox::PosY],
				"Black piano keys must anchor along the top edge of the white keys.");

		// Verify 2D world chassis cutout framing the piano + tight centered oscilloscope dot ribbon
		require(registry.isEntityValid(state.worldCutout) && registry.hasComponent<Shape>(state.worldCutout),
				"InstrumentScene must create a 2D world cutout framing the piano bay.");
		const auto& cut800 = registry.getComponent<Shape>(state.worldCutout);
		const float cutSizeX800 = cut800.parameters[DefaultShapes::RoundedBox::SizeX];
		const float cutSizeY800 = cut800.parameters[DefaultShapes::RoundedBox::SizeY];
		const float cutPosY800 = cut800.parameters[DefaultShapes::RoundedBox::PosY];
		const float camPosY800 = registry.getComponent<Transform>(services.render().getCameraEntity()).position.y;
		require(cut800.distanceFieldId == DefaultShapes::RoundedBox &&
					cut800.combination == CombinationType::Subtraction && cutSizeX800 > cutSizeY800 &&
					cutSizeY800 > 1.0f && cutPosY800 < camPosY800,
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
		require(pressedBlack.parameters[DefaultShapes::RoundBox::PosY] < state.keys[14].basePosY,
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
					state.cachedActionLabels[InstrumentSceneNamespace::State::ACT_MOD] == "--",
				"Lead SoftSine must show SIN and disabled -- in Sound & Timbre slots.");

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
		require(std::abs(cut640.parameters[DefaultShapes::RoundedBox::SizeX] - cutSizeX800) > 0.1f,
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

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
	}

	void testMenuScene(SceneManager& manager, const std::string& name)
	{
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

	struct FloorPlaneSdf : public IMathExpression
	{
		float getValue(const float* parameters) const override
		{
			return parameters[10];
		}

		std::string print() const override
		{
			return "y";
		}
	};

	void testSimulation2DPendingAndCollisionFlags(bool threaded)
	{
		PhysicsSettings settings{};
		settings.runSimulationInThread = threaded;

		Simulation2D sim(64, settings);
		Registry reg;
		reg.registerComponent<RigidBody2D>(std::make_shared<RigidBodyManager>(sim));
		ECS::PhysicsSystem2D::init(reg, sim);

		std::vector<std::shared_ptr<IMathExpression>> sdfs{std::make_shared<FloorPlaneSdf>()};
		sim.setSDFs(sdfs);

		const Entity floorEntity = reg.createEntity();
		Shape floorShape{};
		floorShape.distanceFieldId = 0;
		floorShape.hasCollisions = true;
		sim.updateShape(floorEntity, floorShape);

		struct CallbackState
		{
			std::atomic<size_t> kinematicShapeHits{0};
			SimulationID kinematicId = INVALID_SIMULATION_ID;
		} cbState;

		const Entity shipProbe = reg.createEntity();
		auto& probeT = reg.addComponent<Transform>(shipProbe);
		probeT.position = vec3(0.0f, 0.25f, 0.0f);
		reg.setEntityDirty<Transform>(shipProbe, true);
		auto& probeRb = reg.addComponent<RigidBody2D>(shipProbe);
		probeRb.type = BodyType::Kinematic;
		probeRb.mass = 1.0f;
		probeRb.enableCollision = false;
		probeRb.enableShapeCollision = true;
		reg.setEntityDirty<RigidBody2D>(shipProbe, true);
		cbState.kinematicId = probeRb.simulationId;

		sim.setShapeCollisionCallback(
			[](PhysicsShapeCollisionEvent& ev, void* userData)
			{
				auto* st = static_cast<CallbackState*>(userData);
				if (ev.body == st->kinematicId)
				{
					st->kinematicShapeHits.fetch_add(1, std::memory_order_relaxed);
				}
			},
			&cbState);

		if (threaded)
		{
			sim.startSimulationThread();
		}

		std::vector<Entity> spawnedDots;
		for (int frame = 0; frame < 30; ++frame)
		{
			ECS::PhysicsSystem2D::update(reg, sim);

			for (Entity d : spawnedDots)
			{
				const vec2 pos = vec2(reg.getComponent<Transform>(d).position);
				require(pos.x > 10.0f && glm::length(pos) > 4.0f,
						"Newly spawned rigidbodies must never reset to (0, 0) across physics buffer swaps.");
			}

			sim.update(1.0 / 60.0);
			if (threaded)
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(4));
			}

			if (frame < 25)
			{
				const Entity dot = reg.createEntity();
				auto& t = reg.addComponent<Transform>(dot);
				t.position = vec3(12.0f, 10.0f, 0.0f);
				reg.setEntityDirty<Transform>(dot, true);
				auto& rb = reg.addComponent<RigidBody2D>(dot);
				rb.type = BodyType::Dynamic;
				rb.mass = 0.05f;
				rb.velocity = vec2(3.0f, -24.0f);
				rb.enableCollision = false;
				rb.enableShapeCollision = true;
				reg.setEntityDirty<RigidBody2D>(dot, true);
				spawnedDots.push_back(dot);
			}
		}

		if (threaded)
		{
			sim.stopSimulationThread();
		}

		require(cbState.kinematicShapeHits.load(std::memory_order_relaxed) > 0,
				"Bodies with enableCollision=false and enableShapeCollision=true must emit shape collisions.");
	}

	void testNodeGraphMultiOutputAndMemoization()
	{
		NodeGraph bvGraph;
		int p = bvGraph.addNode("point");
		int bv = bvGraph.addNode("break_vec2");
		int sub = bvGraph.addNode("sub");
		int out = bvGraph.addNode("sdf_output");
		bvGraph.addLink(makePinId(p, false, 0), makePinId(bv, true, 0));
		bvGraph.addLink(makePinId(bv, false, 0), makePinId(sub, true, 0));
		bvGraph.addLink(makePinId(bv, false, 1), makePinId(sub, true, 1));
		bvGraph.addLink(makePinId(sub, false, 0), makePinId(out, true, 0));

		Expr bvExpr = NodeGraphCompiler::evaluate(bvGraph);
		float params[12] = {0.0f};
		params[SystemParams::POINT_X] = 7.5f;
		params[SystemParams::POINT_Y] = -3.25f;
		require(bvExpr.node != nullptr && std::abs(bvExpr.node->getValue(params) - 10.75f) < 1e-5f,
				"break_vec2 must evaluate distinct x (pin 0) and y (pin 1) outputs.");

		std::string bvCpp = NodeGraphCompiler::generateCppCode(bvGraph);
		require(bvCpp.find(".x") != std::string::npos && bvCpp.find(".y") != std::string::npos,
				"break_vec2 C++ code generation must preserve both .x and .y.");

		NodeGraph diamondGraph;
		int prev = diamondGraph.addNode("param_var");
		diamondGraph.getNode(prev)->data.customInt = 0;
		for (int i = 0; i < 32; ++i)
		{
			int next = diamondGraph.addNode("add");
			diamondGraph.addLink(makePinId(prev, false, 0), makePinId(next, true, 0));
			diamondGraph.addLink(makePinId(prev, false, 0), makePinId(next, true, 1));
			prev = next;
		}
		int dOut = diamondGraph.addNode("sdf_output");
		diamondGraph.addLink(makePinId(prev, false, 0), makePinId(dOut, true, 0));

		Expr dExpr = NodeGraphCompiler::evaluate(diamondGraph);
		std::string dCpp = NodeGraphCompiler::generateCppCode(diamondGraph);
		params[0] = 1.0f;
		require(dExpr.node != nullptr && std::abs(dExpr.node->getValue(params) - 4294967296.0f) < 1.0f,
				"Memoized NodeGraphCompiler must evaluate deep diamond DAGs accurately without blowup.");
		require(dCpp.find("auto node") != std::string::npos && dCpp.size() < 8192,
				"Memoized generateCppCode must hoist shared DAG intermediate nodes.");
	}

	void testResourceAndShapeAssetPathResolution()
	{
		ResourceManager resourceMgr;
		ResourceService resources(resourceMgr, ASSETS_PATH);

		const std::string sceneCtx = std::string(ASSETS_PATH) + "aquarium.weirdscene";
		const std::string resolvedRel = resources.resolveAssetPath("clam.weirdshape");
		const std::string resolvedSibling = resources.resolveAssetPath("clam.weirdshape", sceneCtx);
		const std::string resolvedWinLegacy =
			resources.resolveAssetPath("D:\\Projects\\weird-engine\\examples\\sample-scenes\\assets\\Fan.weirdshape");

		require(std::filesystem::exists(resolvedRel) && resolvedRel == resolvedSibling,
				"Relative shape asset paths must resolve cleanly against asset root and scene directory.");
		require(std::filesystem::exists(resolvedWinLegacy) &&
					std::filesystem::path(resolvedWinLegacy).filename() == "Fan.weirdshape",
				"Legacy cross-machine asset paths must resolve via assets/ suffix fallback.");
		require(resources.makePortableAssetPath(resolvedRel, sceneCtx) == "clam.weirdshape",
				"Portable asset path helper must strip local asset base directory.");

		Registry reg;
		PhysicsSettings settings{};
		Simulation2D sim(64, settings);
		std::vector<std::shared_ptr<IMathExpression>> sdfs{std::make_shared<FloorPlaneSdf>()};
		ShapeService shapes(reg, sim, sdfs, &resources);
		ShapeId clamId = shapes.loadCustomShape("clam.weirdshape");
		require(clamId != 0 && shapes.reloadCustomShape("clam.weirdshape"),
				"ShapeService must load and reload custom shapes from relative asset paths.");
		require(shapes.getCustomShapeData("clam.weirdshape") != nullptr,
				"ShapeService cache lookup must match relative and resolved paths consistently.");
	}

	void testTextButtonAndToggleComponents()
	{
		static_assert(WeirdEngine::is_snapshot_serializable_v<TextButton>,
					  "TextButton must remain snapshot-serializable.");
		static_assert(WeirdEngine::is_snapshot_serializable_v<TextToggle>,
					  "TextToggle must remain snapshot-serializable.");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;

		SDFRenderSystemContext uiCtx;
		uiCtx.dotRadious = 5.0f;
		uiCtx.charSpacing = 10.0f;

		// 4 chars ("TEST"): 4 * (3 * 2 * 5) + 3 * 10 = 150px wide, 4 * 2 * 5 = 40px tall
		UITextRenderer leftBottom{};
		leftBottom.text = "TEST";
		leftBottom.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		leftBottom.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		Transform tf{};
		tf.position = vec3(100.0f, 200.0f, 0.0f);

		ButtonSystem::TextBounds bounds{};
		require(ButtonSystem::computeTextBounds(leftBottom, tf, uiCtx, bounds),
				"Non-empty UITextRenderer must produce valid bounds.");
		require(std::abs(bounds.minX - 100.0f) < 0.01f && std::abs(bounds.maxX - 250.0f) < 0.01f &&
					std::abs(bounds.minY - 200.0f) < 0.01f && std::abs(bounds.maxY - 240.0f) < 0.01f,
				"Left-bottom text bounds must match glyph width and height.");
		require(ButtonSystem::isPointInsideText(leftBottom, tf, uiCtx, 255.0f, 245.0f, 8.0f),
				"clickPadding must expand clickable text hit-box.");
		require(!ButtonSystem::isPointInsideText(leftBottom, tf, uiCtx, 260.0f, 220.0f, 8.0f),
				"Points outside clickPadding must not hit text.");

		// Center & Right/Top alignments
		UITextRenderer rightTop = leftBottom;
		rightTop.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
		rightTop.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		require(ButtonSystem::computeTextBounds(rightTop, tf, uiCtx, bounds) &&
					std::abs(bounds.minX - (-50.0f)) < 0.01f && std::abs(bounds.maxX - 100.0f) < 0.01f &&
					std::abs(bounds.minY - 160.0f) < 0.01f && std::abs(bounds.maxY - 200.0f) < 0.01f,
				"Right-top aligned text bounds must anchor relative to top-right edge.");

		UITextRenderer centerAligned = leftBottom;
		centerAligned.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
		centerAligned.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		require(ButtonSystem::computeTextBounds(centerAligned, tf, uiCtx, bounds) &&
					std::abs(bounds.minX - 25.0f) < 0.01f && std::abs(bounds.maxX - 175.0f) < 0.01f &&
					std::abs(bounds.minY - 180.0f) < 0.01f && std::abs(bounds.maxY - 220.0f) < 0.01f,
				"Center aligned text bounds must center around Transform::position.");

		// Verify momentary and toggle state transitions + UI click claiming
		Input::suppressMouseInput();
		ButtonState mState = ButtonState::Off;
		ButtonSystem::stepMomentaryButton(mState, true, true, true);
		require(mState == ButtonState::Down && Input::isUIClick(),
				"Hovered press must enter Down and claim Input::isUIClick().");
		ButtonState blockedState = ButtonState::Off;
		ButtonSystem::stepMomentaryButton(blockedState, true, true, true);
		require(blockedState == ButtonState::Off,
				"Overlapping TextButton must respect an already-claimed Input::isUIClick().");
		ButtonSystem::stepMomentaryButton(mState, true, true, true);
		require(mState == ButtonState::Hold, "Continued press must transition Down -> Hold.");
		ButtonSystem::stepMomentaryButton(mState, true, false, true);
		require(mState == ButtonState::Up, "Releasing mouse must transition Hold -> Up.");
		ButtonSystem::stepMomentaryButton(mState, true, false, true);
		require(mState == ButtonState::Off, "Subsequent frame must settle Up -> Off.");

		ButtonState tState = ButtonState::Off;
		bool tActive = false;
		Input::suppressMouseInput();
		Input::clearUIClick();
		ButtonSystem::stepToggleState(tState, tActive, true, true, false);
		require(tActive && tState == ButtonState::Down, "Toggle click must activate and enter Down.");
		ButtonSystem::stepToggleState(tState, tActive, true, false, false);
		require(tActive && tState == ButtonState::Hold, "Active toggle must advance Down -> Hold.");
		ButtonSystem::stepToggleState(tState, tActive, true, true, false);
		require(!tActive && tState == ButtonState::Up, "Second click must deactivate toggle to Up.");
		ButtonSystem::stepToggleState(tState, tActive, true, false, false);
		require(!tActive && tState == ButtonState::Off, "Deactivated toggle must settle Up -> Off.");
		Input::clearUIClick();

		// Verify Registry updateTextButtons + updateTextToggles hover detection and material updates
		Registry reg;
		reg.registerComponent<Transform>();
		reg.registerComponent<UITextRenderer>();
		reg.registerComponent<TextButton>();
		reg.registerComponent<TextToggle>();

		const Entity btnEntity = reg.createEntity();
		reg.addComponent<Transform>(btnEntity).position = vec3(100.0f, 300.0f, 0.0f);
		auto& btnText = reg.addComponent<UITextRenderer>(btnEntity);
		btnText.text = "PLAY";
		btnText.material = 2;
		auto& txtBtn = reg.addComponent<TextButton>(btnEntity);
		txtBtn.clickPadding = 6.0f;
		txtBtn.hoverMaterial = 5;

		const Entity tglEntity = reg.createEntity();
		reg.addComponent<Transform>(tglEntity).position = vec3(500.0f, 300.0f, 0.0f);
		auto& tglText = reg.addComponent<UITextRenderer>(tglEntity);
		tglText.text = "MUTE";
		tglText.material = 3;
		auto& txtTgl = reg.addComponent<TextToggle>(tglEntity);
		txtTgl.active = false;
		txtTgl.activeMaterial = 7;
		txtTgl.inactiveMaterial = 3;
		txtTgl.hoverMaterial = 8;

		// Input::GetMouseY() = Display::height - m_mouseY, so screen UI Y=310 maps to SDL Y=490 at 800px tall
		Input::SetMousePosition(140.0f, 490.0f);
		ButtonSystem::updateTextButtons(reg, uiCtx);
		ButtonSystem::updateTextToggles(reg, uiCtx);
		require(txtBtn.hovered && btnText.material == 5 && !txtTgl.hovered && tglText.material == 3,
				"Hovering TextButton must swap to hoverMaterial while leaving unhovered TextToggle inactiveMaterial.");

		Input::SetMousePosition(540.0f, 490.0f);
		ButtonSystem::updateTextButtons(reg, uiCtx);
		ButtonSystem::updateTextToggles(reg, uiCtx);
		require(!txtBtn.hovered && btnText.material == 2 && txtTgl.hovered && tglText.material == 8,
				"Leaving TextButton must restore baseMaterial and hovering TextToggle must apply hoverMaterial.");

		Input::SetMousePosition(0.0f, 0.0f);
	}

	void testBuiltinRotatedAndRoundedShapes()
	{
		Scene::registerBuiltinSDFs();
		auto builtins = Scene::getBuiltinSDFs();
		require(builtins.size() >= DefaultShapes::COUNT_2D, "Builtin SDF table must cover all 2D DefaultShapes.");

		auto evalBuiltin = [&](ShapeId id, const float(&shapeVars)[8], vec2 p) -> float
		{
			const auto& expr = builtins[id];
			require(expr != nullptr, "Builtin shape AST must not be null.");
			float params[12]{};
			packSdfParameters(params, shapeVars, 0.0f, p, 0.0f);
			float val = expr->getValue(params);
			require(std::isfinite(val), "Builtin shape distance must be finite.");
			return val;
		};

		// BoxRotated: halfSize=(2, 1), angle=pi/4 around origin -> (sqrt(2), 0) has distance < 0, (2.2, 0) > 0
		constexpr float kPiOver4 = 0.78539816339f;
		constexpr float kPiOver2 = 1.57079632679f;
		const float rotBoxVars[8] = {0.0f, 0.0f, 2.0f, 1.0f, kPiOver2, 0.0f, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::BoxRotated, rotBoxVars, {0.5f, 1.5f}) < 0.0f &&
					evalBuiltin(DefaultShapes::BoxRotated, rotBoxVars, {1.5f, 0.5f}) > 0.0f,
				"BoxRotated at 90deg must swap X/Y extents.");

		// TriangleRotated: 90deg rotation places apex along X instead of +Y
		const float rotTriVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, kPiOver4, 0.0f, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::TriangleRotated, rotTriVars, {0.0f, 0.0f}) < 0.0f,
				"TriangleRotated must contain its local origin.");

		// RoundTriangle: unrounded (r=0) matches Triangle; rounded (r=0.25) preserves flat bottom & rounds apex
		const float triVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f};
		const float roundTriVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 0.25f, 0.0f, 0.0f, 0.0f};
		const float sharpApex = evalBuiltin(DefaultShapes::Triangle, triVars, {0.0f, 4.0f / 3.0f});
		const float zeroRadApex = evalBuiltin(DefaultShapes::RoundTriangle, triVars, {0.0f, 4.0f / 3.0f});
		require(std::abs(sharpApex) < 1e-4f && std::abs(zeroRadApex) < 1e-4f,
				"RoundTriangle with radius=0 must match Triangle.");

		const float roundedBottomMid = evalBuiltin(DefaultShapes::RoundTriangle, roundTriVars, {0.0f, -2.0f / 3.0f});
		const float roundedApex = evalBuiltin(DefaultShapes::RoundTriangle, roundTriVars, {0.0f, 4.0f / 3.0f});
		require(std::abs(roundedBottomMid) < 1e-3f && roundedApex > 0.25f,
				"RoundTriangle must keep the bottom midpoint on the boundary and pull back the sharp apex.");

		// Excessive radius must clamp cleanly without NaN and remain inside/tangent to the triangle
		const float clampedVars[8] = {0.0f, 0.0f, 2.0f, 2.0f, 50.0f, 0.0f, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::RoundTriangle, clampedVars, {0.0f, -2.0f / 3.0f + 0.618f}) < 0.0f &&
					evalBuiltin(DefaultShapes::RoundTriangle, clampedVars, {0.0f, 1.3f}) > 0.0f,
				"RoundTriangle must clamp oversized radius to inscribed circle without NaN.");

		// RoundTriangleRotated: verify GLSL print and rotation
		const float rotRoundTriVars[8] = {1.0f, -2.0f, 2.0f, 2.0f, 0.25f, kPiOver2, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::RoundTriangleRotated, rotRoundTriVars, {1.0f, -2.0f}) < 0.0f,
				"RoundTriangleRotated must contain its translated center.");
		std::string glsl = builtins[DefaultShapes::RoundTriangle]->print();
		require(!glsl.empty() && glsl.find("nan") == std::string::npos,
				"RoundTriangle GLSL expression generation must succeed.");

		// RoundBoxRotated: 90deg rotation swaps SizeX/SizeY and rounds corners
		const float rotRoundBoxVars[8] = {1.0f, -2.0f, 2.0f, 1.0f, 0.3f, kPiOver2, 0.0f, 0.0f};
		require(evalBuiltin(DefaultShapes::RoundBoxRotated, rotRoundBoxVars, {1.5f, -0.5f}) < 0.0f &&
					evalBuiltin(DefaultShapes::RoundBoxRotated, rotRoundBoxVars, {2.5f, -1.5f}) > 0.0f &&
					evalBuiltin(DefaultShapes::RoundBoxRotated, rotRoundBoxVars, {2.0f, 0.0f}) > 0.0f,
				"RoundBoxRotated must rotate extents and round sharp corners.");

		// Verify DefaultShapes::Star exposes its named parameter indices
		static_assert(DefaultShapes::Star::PosX == 0 && DefaultShapes::Star::PosY == 1 &&
					  DefaultShapes::Star::Radius == 2 && DefaultShapes::Star::Displacement == 3 &&
					  DefaultShapes::Star::Points == 4 && DefaultShapes::Star::Speed == 5);

		// Verify round_triangle NodeGraph node compiles cleanly
		NodeGraph graph;
		int pNode = graph.addNode("point", {0.0f, 0.0f});
		int triNode = graph.addNode("round_triangle", {120.0f, 0.0f});
		int outNode = graph.addNode("sdf_output", {260.0f, 0.0f});
		graph.addLink(makePinId(pNode, false, 0), makePinId(triNode, true, 0));
		graph.addLink(makePinId(triNode, false, 0), makePinId(outNode, true, 0));
		Expr compiled = NodeGraphCompiler::evaluate(graph);
		require(compiled.node != nullptr && !compiled.node->print().empty(),
				"NodeGraph round_triangle node must evaluate and print GLSL.");
	}

	void testGpuShape2DPackingParity()
	{
		Scene::registerBuiltinSDFs();
		const auto builtins = Scene::getBuiltinSDFs();
		constexpr float kTime = 1.37f;

		// CPU mirror of the 7 collapsed GLSL evaluators in shaders/2d/sdf_shapes.frag
		auto evalPackedBuiltin = [&](const vec4& meta, const vec4& p0, const vec4& p1, vec2 p) -> float
		{
			const uint32_t packed0 = static_cast<uint32_t>(meta.x + 0.5f);
			const uint16_t shapeId = static_cast<uint16_t>(packed0 & 0xFFFFu);
			const uint32_t flags = packed0 >> 19u;
			const float onionThickness = meta.w;

			float dist = 10000.0f;
			switch (shapeId)
			{
				case GpuShape2D::Circle:
					dist = glm::length(p - vec2(p0.x, p0.y)) - p0.z;
					break;
				case GpuShape2D::Box:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					vec2 d = glm::abs(q) - vec2(p0.z, p0.w);
					dist = glm::length(glm::max(d, vec2(0.0f))) + std::min(std::max(d.x, d.y), 0.0f) - p1.z;
					break;
				}
				case GpuShape2D::Triangle:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.x * q.x - p1.y * q.y, p1.y * q.x + p1.x * q.y);
					q.y -= p1.w;
					const float w = p0.z;
					const float h = p0.w;
					const vec2 v0(-w * 0.5f, -h / 3.0f);
					const vec2 v1(w * 0.5f, -h / 3.0f);
					const vec2 v2(0.0f, 2.0f * h / 3.0f);
					const vec2 e0 = v1 - v0, e1 = v2 - v1, e2 = v0 - v2;
					const vec2 d0 = q - v0, d1 = q - v1, d2 = q - v2;
					const vec2 pq0 =
						d0 - e0 * std::clamp(glm::dot(d0, e0) / std::max(glm::dot(e0, e0), 1e-12f), 0.0f, 1.0f);
					const vec2 pq1 =
						d1 - e1 * std::clamp(glm::dot(d1, e1) / std::max(glm::dot(e1, e1), 1e-12f), 0.0f, 1.0f);
					const vec2 pq2 =
						d2 - e2 * std::clamp(glm::dot(d2, e2) / std::max(glm::dot(e2, e2), 1e-12f), 0.0f, 1.0f);
					const float s = (e0.x * e2.y - e0.y * e2.x) >= 0.0f ? 1.0f : -1.0f;
					const float dx = std::min({glm::dot(pq0, pq0), glm::dot(pq1, pq1), glm::dot(pq2, pq2)});
					const float dy = std::min({s * (d0.x * e0.y - d0.y * e0.x), s * (d1.x * e1.y - d1.y * e1.x),
											   s * (d2.x * e2.y - d2.y * e2.x)});
					const float signDy = (dy > 0.0f) ? 1.0f : ((dy < 0.0f) ? -1.0f : 0.0f);
					dist = -std::sqrt(dx) * signDy - p1.z;
					break;
				}
				case GpuShape2D::Line:
				{
					const vec2 pa = p - vec2(p0.x, p0.y);
					const vec2 ba(p0.z, p0.w);
					const float h = std::clamp(glm::dot(pa, ba) * p1.x, 0.0f, 1.0f);
					dist = glm::length(pa - ba * h) - p1.y;
					break;
				}
				case GpuShape2D::Ramp:
				{
					vec2 q = p - vec2(p0.x, p0.y);
					q = vec2(p1.y * q.x - p1.z * q.y, p1.z * q.x + p1.y * q.y);
					const float wi = p0.z;
					const float he = p0.w;
					const float sk = p1.x;
					const vec2 e(wi, sk);
					q *= (q.x >= 0.0f ? 1.0f : -1.0f);
					const vec2 w = q - e;
					const vec2 wClamped(w.x, w.y - std::clamp(w.y, -he, he));
					const float dx0 = glm::dot(wClamped, wClamped);
					const float dy0 = -w.x;
					const float s = q.y * e.x - q.x * e.y;
					q *= (s >= 0.0f ? 1.0f : -1.0f);
					vec2 v = q - vec2(0.0f, he);
					const float h = std::clamp(glm::dot(v, e) * p1.w, -1.0f, 1.0f);
					v -= e * h;
					const float dx1 = glm::dot(v, v);
					const float dy1 = wi * he - std::abs(s);
					const float mdy = -std::min(dy0, dy1);
					const float signMdy = (mdy > 0.0f) ? 1.0f : ((mdy < 0.0f) ? -1.0f : 0.0f);
					dist = std::sqrt(std::min(dx0, dx1)) * signMdy;
					break;
				}
				case GpuShape2D::SineWave:
					dist = (p.y - p0.w) - p0.x * std::sin(p0.y * p.x + p0.z * kTime);
					break;
				case GpuShape2D::Star:
				{
					const vec2 q = p - vec2(p0.x, p0.y);
					const float d = glm::length(q) - p0.z;
					const float angle = p1.x * std::atan2(q.y, q.x) - p1.y * kTime;
					dist = d + p0.w * std::sin(angle);
					break;
				}
				default:
					break;
			}

			if ((flags & GpuShape2D::FLAG_ONION) != 0u)
			{
				dist = std::abs(dist) - onionThickness;
			}
			return dist;
		};

		struct BuiltinCase
		{
			ShapeId id;
			float vars[8];
		};

		const BuiltinCase cases[] = {
			{DefaultShapes::Circle, {1.2f, -0.7f, 2.5f, 0, 0, 0, 0, 0}},
			{DefaultShapes::CircleLine, {1.2f, -0.7f, 2.5f, 0.25f, 0, 0, 0, 0}},
			{DefaultShapes::Box, {-1.5f, 0.8f, 2.2f, 1.1f, 0, 0, 0, 0}},
			{DefaultShapes::BoxLine, {-1.5f, 0.8f, 2.2f, 1.1f, 0.2f, 0, 0, 0}},
			{DefaultShapes::RoundBox, {0.4f, -1.1f, 2.0f, 1.5f, 0.35f, 0, 0, 0}},
			{DefaultShapes::RoundBoxLine, {0.4f, -1.1f, 2.0f, 1.5f, 0.35f, 0.15f, 0, 0}},
			{DefaultShapes::RoundedBox, {-0.3f, 0.9f, 1.8f, 1.2f, 0.4f, 0, 0, 0}},
			{DefaultShapes::RoundedBoxLine, {-0.3f, 0.9f, 1.8f, 1.2f, 0.4f, 0.12f, 0, 0}},
			{DefaultShapes::Triangle, {0.5f, -0.4f, 2.6f, 2.1f, 0, 0, 0, 0}},
			{DefaultShapes::TriangleLine, {0.5f, -0.4f, 2.6f, 2.1f, 0.18f, 0, 0, 0}},
			{DefaultShapes::Line, {-2.0f, -1.0f, 3.0f, 1.5f, 0.4f, 0, 0, 0}},
			{DefaultShapes::Ramp, {0.2f, 0.3f, 2.0f, 1.2f, 0.6f, 0, 0, 0}},
			{DefaultShapes::SineWave, {0.8f, 1.5f, 2.2f, -0.5f, 0, 0, 0, 0}},
			{DefaultShapes::Star, {-0.6f, 0.7f, 2.0f, 0.45f, 5.0f, 1.3f, 0, 0}},
			{DefaultShapes::BoxRotated, {1.0f, -1.0f, 2.4f, 0.9f, 0.65f, 0, 0, 0}},
			{DefaultShapes::BoxLineRotated, {1.0f, -1.0f, 2.4f, 0.9f, 0.65f, 0.18f, 0, 0}},
			{DefaultShapes::TriangleRotated, {-0.8f, 0.5f, 2.5f, 1.9f, -0.9f, 0, 0, 0}},
			{DefaultShapes::TriangleLineRotated, {-0.8f, 0.5f, 2.5f, 1.9f, -0.9f, 0.15f, 0, 0}},
			{DefaultShapes::RampRotated, {0.7f, -0.2f, 1.8f, 1.1f, -0.5f, 0.8f, 0, 0}},
			{DefaultShapes::RoundTriangle, {0.3f, 0.4f, 2.4f, 2.0f, 0.3f, 0, 0, 0}},
			{DefaultShapes::RoundTriangle, {0.3f, 0.4f, 2.4f, -2.0f, 0.3f, 0, 0, 0}},
			{DefaultShapes::RoundTriangleRotated, {-0.5f, -0.6f, 2.2f, -1.8f, 0.25f, 1.1f, 0, 0}},
			{DefaultShapes::RoundBoxRotated, {0.9f, 0.2f, 2.1f, 1.3f, 0.35f, -0.7f, 0, 0}},
		};

		const vec2 samplePoints[] = {
			{0.0f, 0.0f}, {1.2f, -0.7f}, {-2.3f, 1.8f}, {3.1f, -2.4f}, {0.35f, 0.95f}, {-0.9f, -1.6f},
		};

		for (const auto& tc : cases)
		{
			vec4 meta, p0, p1;
			GpuShape2D::packInstance(tc.id, CombinationType::Addition, 0, 3, 1.0f, tc.vars, meta, p0, p1);
			for (const vec2& pt : samplePoints)
			{
				float fullParams[12];
				packSdfParameters(fullParams, tc.vars, kTime, pt, 0.0f);
				const float refDist = builtins[tc.id]->getValue(fullParams);
				const float gpuDist = evalPackedBuiltin(meta, p0, p1, pt);
				require(std::isfinite(gpuDist) && std::abs(refDist - gpuDist) < 1e-4f,
						"Collapsed GPU builtin evaluator must match reference AST SDF for all 22 built-in shapes.");
			}
		}

		// Verify metadata bit packing, custom shape passthrough, and group sorting in packShapes
		Registry reg;
		reg.registerComponent<Shape>();
		const Entity eGlobal = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eGlobal);
			s.distanceFieldId = DefaultShapes::Circle;
			s.combination = CombinationType::SmoothSubtraction;
			s.groupIdx = Shape::GLOBAL_GROUP;
			s.material = 12;
			s.smoothFactor = 2.5f;
		}
		const Entity eGroup2 = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eGroup2);
			s.distanceFieldId = static_cast<ShapeId>(DefaultShapes3D::TOTAL_BUILTIN_SHAPES + 3);
			s.combination = CombinationType::SmoothAddition;
			s.groupIdx = 2;
			s.material = 7;
			s.smoothFactor = 0.75f;
			s.parameters[0] = 42.0f;
			s.parameters[7] = -9.5f;
		}
		const Entity eGroup0 = reg.createEntity();
		{
			auto& s = reg.addComponent<Shape>(eGroup0);
			s.distanceFieldId = DefaultShapes::BoxLine;
			s.combination = CombinationType::Addition;
			s.groupIdx = 0;
			s.material = 15;
			s.parameters[DefaultShapes::BoxLine::Thickness] = 0.3f;
		}

		std::vector<vec4> packed;
		std::vector<size_t> scratch;
		uint32_t shapeCount = 0;
		GpuShape2D::packShapes<Shape>(reg, packed, scratch, shapeCount);

		require(shapeCount == 3 && packed.size() == 9, "packShapes must emit 3 vec4s per shape.");

		auto unpack = [&](size_t idx)
		{
			const vec4& m = packed[idx * 3];
			const uint32_t p0Bits = static_cast<uint32_t>(m.x + 0.5f);
			const uint32_t p1Bits = static_cast<uint32_t>(m.y + 0.5f);
			struct Unpacked
			{
				uint16_t shapeId;
				uint16_t comb;
				uint32_t flags;
				uint16_t group;
				uint16_t mat;
				float smooth;
				float onion;
			};
			return Unpacked{static_cast<uint16_t>(p0Bits & 0xFFFFu),
							static_cast<uint16_t>((p0Bits >> 16u) & 0x7u),
							p0Bits >> 19u,
							static_cast<uint16_t>(p1Bits & 0xFFFFu),
							static_cast<uint16_t>((p1Bits >> 16u) & 0x3Fu),
							m.z,
							m.w};
		};

		const auto u0 = unpack(0);
		const auto u1 = unpack(1);
		const auto u2 = unpack(2);
		require(u0.group == 0 && u0.shapeId == GpuShape2D::Box && (u0.flags & GpuShape2D::FLAG_ONION) != 0u &&
					u0.mat == 15 && std::abs(u0.onion - 0.3f) < 1e-5f,
				"First sorted shape must be group 0 BoxLine collapsed to Box with FLAG_ONION.");
		require(u1.group == 2 && u1.shapeId == DefaultShapes3D::TOTAL_BUILTIN_SHAPES + 3 &&
					u1.comb == static_cast<uint16_t>(CombinationType::SmoothAddition) && u1.mat == 7 &&
					std::abs(u1.smooth - 0.75f) < 1e-5f && packed[4].x == 42.0f && packed[5].w == -9.5f,
				"Second sorted shape must preserve custom ShapeId and raw parameters.");
		require(u2.group == Shape::GLOBAL_GROUP && (u2.flags & GpuShape2D::FLAG_GLOBAL_GROUP) != 0u &&
					u2.shapeId == GpuShape2D::Circle &&
					u2.comb == static_cast<uint16_t>(CombinationType::SmoothSubtraction) && u2.mat == 12,
				"GLOBAL_GROUP shape must sort last with FLAG_GLOBAL_GROUP set.");

		// Verify ShapeService increments sdfDefinitionsVersion on registerSDF
		uint64_t defVersion = 1;
		PhysicsSettings settings{};
		Simulation2D sim(16, settings);
		std::vector<std::shared_ptr<IMathExpression>> sdfs(builtins.begin(), builtins.end());
		ShapeService shapeService(reg, sim, sdfs, nullptr, &defVersion);
		shapeService.registerSDF(SDF::sdCircle(SDF::point(), 1.0f));
		require(defVersion == 2, "ShapeService::registerSDF must increment sdfDefinitionsVersion.");
	}

	void testPlatformerWorldClusterFixedBody(SceneManager& manager, const std::string& name)
	{
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(name, ASSETS_PATH "platformer_world.weirdscene");
		manager.loadScene(name);

		auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
		auto& registry = *scene.registry;
		auto& services = *scene.services;

		ClusterInstance* ameba = services.clusters().getByName("ameba_1");
		ClusterInstance* man = services.clusters().getByName("man_2");
		require(ameba != nullptr && man != nullptr,
				"platformer_world.weirdscene must load clusters ameba_1 and man_2.");

		const uint32_t amebaId = ameba->id;
		const uint32_t manId = man->id;

		auto countFixedInCluster = [&](const ClusterInstance& cluster, Entity& outSampleEntity, vec3& outSamplePos)
		{
			size_t count = 0;
			outSampleEntity = INVALID_ENTITY;
			for (Entity e : cluster.entities)
			{
				if (!registry.hasComponent<RigidBody2D>(e))
					continue;
				const auto& rb = registry.getComponent<RigidBody2D>(e);
				if (rb.type == BodyType::Fixed || rb.isFixed)
				{
					++count;
					outSampleEntity = e;
					outSamplePos = registry.getComponent<Transform>(e).position;
				}
			}
			return count;
		};

		Entity fixedAmebaEnt = INVALID_ENTITY;
		vec3 amebaFixedPosBefore(0.0f);
		require(countFixedInCluster(*ameba, fixedAmebaEnt, amebaFixedPosBefore) == 1,
				"ameba_1 in platformer_world.weirdscene must start with 1 Fixed anchor.");

		Entity fixedManSample = INVALID_ENTITY;
		vec3 manFixedSamplePos(0.0f);
		require(countFixedInCluster(*man, fixedManSample, manFixedSamplePos) == 6,
				"man_2 in platformer_world.weirdscene must start with 6 Fixed vertices.");

		// Reload both clusters directly from their .weirdcluster assets (whose bodies are all Dynamic):
		// scene overrides must survive cluster reloads even when template entity ordering differs.
		require(services.clusters().reloadInstance(amebaId, ASSETS_PATH "Organisms/ameba.weirdcluster"),
				"ameba_1 must reload from ameba.weirdcluster.");
		require(services.clusters().reloadInstance(manId, ASSETS_PATH "Organisms/man.weirdcluster"),
				"man_2 must reload from man.weirdcluster.");

		ameba = services.clusters().getById(amebaId);
		man = services.clusters().getById(manId);
		require(ameba != nullptr && man != nullptr, "Reloaded clusters must remain registered.");

		Entity fixedAmebaAfter = INVALID_ENTITY;
		vec3 amebaFixedPosAfter(0.0f);
		require(countFixedInCluster(*ameba, fixedAmebaAfter, amebaFixedPosAfter) == 1,
				"ameba_1 must keep its Fixed anchor after reloading from ameba.weirdcluster.");
		require(glm::distance(vec2(amebaFixedPosAfter), vec2(amebaFixedPosBefore)) < 0.1f,
				"ameba_1 Fixed anchor must map back to the same corner vertex after scrambled reload.");
		require(ameba->hasOverrideMetadata &&
					(ameba->getOverrides(fixedAmebaAfter) & ClusterOverrideFlag::BodyType) != 0,
				"ameba_1 reload must promote inferred Fixed override into explicit override metadata.");

		Entity fixedManAfter = INVALID_ENTITY;
		require(countFixedInCluster(*man, fixedManAfter, manFixedSamplePos) == 6,
				"man_2 must keep all 6 Fixed vertices after reloading from man.weirdcluster.");
		const Entity manHeadAfter = man->getEntity("head");
		require(manHeadAfter != INVALID_ENTITY && registry.hasComponent<RigidBody2D>(manHeadAfter) &&
					registry.getComponent<RigidBody2D>(manHeadAfter).type == BodyType::Fixed,
				"man_2 local-named 'head' must remain BodyType::Fixed after cluster reload.");

		// Second reload + snapshot roundtrip must preserve the Fixed anchor idempotently.
		require(services.clusters().reloadInstance(amebaId, ASSETS_PATH "Organisms/ameba.weirdcluster"),
				"ameba_1 second reload must succeed.");
		ameba = services.clusters().getById(amebaId);
		require(countFixedInCluster(*ameba, fixedAmebaAfter, amebaFixedPosAfter) == 1,
				"ameba_1 must retain Fixed anchor across repeated reloads.");

		SnapshotFile saved;
		services.serialization().saveSnapshot(saved.path.string());
		require(std::filesystem::exists(saved.path), "platformer_world snapshot must save.");

		auto& mutRb = registry.getComponent<RigidBody2D>(fixedAmebaAfter);
		mutRb.type = BodyType::Dynamic;
		mutRb.isFixed = false;
		require(services.serialization().loadSnapshot(saved.path.string()),
				"platformer_world snapshot must restore cleanly.");

		const auto& restoredRb = registry.getComponent<RigidBody2D>(fixedAmebaAfter);
		const auto& restoredPos = registry.getComponent<Transform>(fixedAmebaAfter).position;
		require(restoredRb.type == BodyType::Fixed && restoredRb.isFixed,
				"ameba_1 anchor rigidbody must remain BodyType::Fixed after snapshot restore.");
		require(glm::length(restoredPos - amebaFixedPosAfter) < 1e-4f,
				"ameba_1 Fixed anchor position must remain unchanged.");
	}

	void testClusterReloadPreservesSceneOverrides(SceneManager& manager, const std::string& namePrefix)
	{
		struct TempAssetFile
		{
			std::filesystem::path path;
			explicit TempAssetFile(const std::string& ext)
			{
				static size_t seq = 0;
				const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
				path = std::filesystem::temp_directory_path() /
					   ("weird-cluster-override-" + std::to_string(tick) + "-" + std::to_string(seq++) + ext);
			}
			~TempAssetFile()
			{
				std::error_code ec;
				std::filesystem::remove(path, ec);
			}
		};

		TempAssetFile clusterFile(".weirdcluster");
		TempAssetFile sceneFile(".weirdscene");

		// v1 cluster asset: two dots ("anchor" at (0,0) and free dot at (2,0)), all Dynamic, material 1, mass 1.0
		{
			nlohmann::json cj = {
				{"version", 1},
				{"clusters", nlohmann::json::array()},
				{"entities",
				 {{{"id", 10},
				   {"dot", {{"isStatic", false}, {"materialId", 1}}},
				   {"rigidBody2D",
					{{"bodyType", 0},
					 {"enableCollision", true},
					 {"enableShapeCollision", true},
					 {"mass", 1.0f},
					 {"physicsPosition", {0.0f, 0.0f}},
					 {"simulationId", 0},
					 {"velocity", {0.0f, 0.0f}}}},
				   {"transform",
					{{"position", {0.0f, 0.0f, 0.0f}},
					 {"rotation", {0.0f, 0.0f, 0.0f}},
					 {"scale", {1.0f, 1.0f, 1.0f}}}}},
				  {{"id", 20},
				   {"dot", {{"isStatic", false}, {"materialId", 1}}},
				   {"rigidBody2D",
					{{"bodyType", 0},
					 {"enableCollision", true},
					 {"enableShapeCollision", true},
					 {"mass", 1.0f},
					 {"physicsPosition", {2.0f, 0.0f}},
					 {"simulationId", 1},
					 {"velocity", {0.0f, 0.0f}}}},
				   {"transform",
					{{"position", {2.0f, 0.0f, 0.0f}},
					 {"rotation", {0.0f, 0.0f, 0.0f}},
					 {"scale", {1.0f, 1.0f, 1.0f}}}}}}},
				{"names", {{{"name", "anchor"}, {"entityId", 10}}}},
				{"entityTags", nlohmann::json::array()},
				{"physics",
				 {{"distanceConstraints", nlohmann::json::array()},
				  {"springs", {{{"entity", 30}, {"entityA", 10}, {"entityB", 20}, {"distance", 2.0f}, {"k", 0.5f}}}}}},
				{"materials2D", nlohmann::json::array()}};
			std::ofstream out(clusterFile.path);
			out << cj.dump(2);
		}

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-a");
		manager.loadScene(namePrefix + "-a");

		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			ClusterInstance& cluster = services.clusters().instantiate(clusterFile.path.string(), vec2(5.0f, 1.0f));
			const uint32_t clusterId = cluster.id;
			const Entity anchor = cluster.getEntity("anchor");
			require(anchor != INVALID_ENTITY, "Cluster must resolve local name 'anchor'.");

			// Add an external scene entity attached to 'anchor' via a Spring
			const Entity extAnchor = registry.createEntity();
			registry.addComponent<Transform>(extAnchor).position = vec3(5.0f, 4.0f, 0.0f);
			registry.addComponent<RigidBody2D>(extAnchor).type = BodyType::Fixed;
			const Entity extSpringEnt = registry.createEntity();
			auto& extSpring = registry.addComponent<Spring>(extSpringEnt);
			extSpring.entityA = extAnchor;
			extSpring.entityB = anchor;
			extSpring.restDistance = 2.5f;
			extSpring.stiffness = 0.75f;

			// Override multiple properties on 'anchor' in the scene
			auto& rb = registry.getComponent<RigidBody2D>(anchor);
			rb.type = BodyType::Fixed;
			rb.isFixed = true;
			rb.mass = 4.5f;
			rb.enableCollision = false;
			registry.getComponent<Dot>(anchor).materialId = 9;
			registry.getComponent<Transform>(anchor).position = vec3(5.0f, 2.5f, 0.0f);
			services.names().setName(anchor, "PinnedBridgeAnchor");
			services.tags().addTag(anchor, "bridge_pin");
			services.clusters().markEntityOverride(
				anchor, ClusterOverrideFlag::BodyType | ClusterOverrideFlag::Mass |
							ClusterOverrideFlag::EnableCollision | ClusterOverrideFlag::DotMaterial |
							ClusterOverrideFlag::Position | ClusterOverrideFlag::GlobalName |
							ClusterOverrideFlag::CustomTags);

			SceneSerializer::save(scene, sceneFile.path.string());

			// Verify reload inside the current scene updates un-overridden fields and keeps all overrides
			// First write v2 cluster asset: reversed entity order, dot 20 material->7 & mass->3.0, plus new dot 25
			{
				nlohmann::json cj2 = {
					{"version", 1},
					{"clusters", nlohmann::json::array()},
					{"entities",
					 {{{"id", 20},
					   {"dot", {{"isStatic", false}, {"materialId", 7}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 3.0f},
						 {"physicsPosition", {2.0f, 0.0f}},
						 {"simulationId", 0},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {2.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}},
					  {{"id", 25},
					   {"dot", {{"isStatic", false}, {"materialId", 6}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 1.0f},
						 {"physicsPosition", {4.0f, 0.0f}},
						 {"simulationId", 1},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {4.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}},
					  {{"id", 10},
					   {"dot", {{"isStatic", false}, {"materialId", 1}}},
					   {"rigidBody2D",
						{{"bodyType", 0},
						 {"enableCollision", true},
						 {"enableShapeCollision", true},
						 {"mass", 1.0f},
						 {"physicsPosition", {0.0f, 0.0f}},
						 {"simulationId", 2},
						 {"velocity", {0.0f, 0.0f}}}},
					   {"transform",
						{{"position", {0.0f, 0.0f, 0.0f}},
						 {"rotation", {0.0f, 0.0f, 0.0f}},
						 {"scale", {1.0f, 1.0f, 1.0f}}}}}}},
					{"names", {{{"name", "anchor"}, {"entityId", 10}}}},
					{"entityTags", nlohmann::json::array()},
					{"physics",
					 {{"distanceConstraints", nlohmann::json::array()},
					  {"springs",
					   {{{"entity", 30}, {"entityA", 10}, {"entityB", 20}, {"distance", 2.0f}, {"k", 0.5f}}}}}},
					{"materials2D", nlohmann::json::array()}};
				std::ofstream out(clusterFile.path);
				out << cj2.dump(2);
			}

			require(services.clusters().reloadInstance(clusterId, clusterFile.path.string()),
					"Cluster reload from v2 asset must succeed.");
			require(registry.isEntityAlive(extSpringEnt) && registry.hasComponent<Spring>(extSpringEnt),
					"External spring attached to cluster member must survive cluster reload.");
			const Entity newAnchor = cluster.getEntity("anchor");
			require(registry.getComponent<Spring>(extSpringEnt).entityB == newAnchor,
					"Surviving external spring must rebind to the reloaded cluster member.");
		}

		// Now load the previously saved .weirdscene (with persisted overrides & sourceEntityIds) into a fresh Scene,
		// then reload its cluster against v2 asset and verify everything survives save/load + reload.
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-b", sceneFile.path.string());
		manager.loadScene(namePrefix + "-b");
		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			require(services.clusters().getAll().size() == 1, "Saved scene must restore cluster instance.");
			const uint32_t clusterId = services.clusters().getAll().front().id;
			require(services.clusters().reloadInstance(clusterId, clusterFile.path.string()),
					"Cluster reload after scene save/load must succeed.");

			ClusterInstance* cluster = services.clusters().getById(clusterId);
			require(cluster != nullptr, "Reloaded cluster must exist.");
			const Entity anchor = cluster->getEntity("anchor");
			require(anchor != INVALID_ENTITY, "Reloaded cluster must retain 'anchor' localName.");

			const auto& rbAnchor = registry.getComponent<RigidBody2D>(anchor);
			require(rbAnchor.type == BodyType::Fixed && rbAnchor.isFixed,
					"BodyType override on cluster member must survive save/load + reload.");
			require(std::abs(rbAnchor.mass - 4.5f) < 1e-4f, "Mass override must survive reload.");
			require(!rbAnchor.enableCollision && rbAnchor.enableShapeCollision,
					"Collision flag override must survive reload.");
			require(registry.getComponent<Dot>(anchor).materialId == 9, "Dot material override must survive reload.");
			require(std::abs(registry.getComponent<Transform>(anchor).position.y - 2.5f) < 1e-3f,
					"Relative position override must survive reload.");
			require(services.names().getEntityByName("PinnedBridgeAnchor") == anchor,
					"Global entity name override must survive reload.");
			require(services.tags().hasTag(anchor, "bridge_pin"), "Custom entity tag override must survive reload.");

			int dotMat7Count = 0;
			int dotMat6Count = 0;
			bool updatedUnoverriddenMass = false;
			for (Entity e : cluster->entities)
			{
				if (!registry.hasComponent<Dot>(e) || e == anchor)
					continue;
				const auto& d = registry.getComponent<Dot>(e);
				if (d.materialId == 7)
				{
					++dotMat7Count;
					if (std::abs(registry.getComponent<RigidBody2D>(e).mass - 3.0f) < 1e-4f)
						updatedUnoverriddenMass = true;
				}
				else if (d.materialId == 6)
				{
					++dotMat6Count;
				}
			}
			require(dotMat7Count == 1 && updatedUnoverriddenMass,
					"Un-overridden cluster member must pull updated material & mass from reloaded asset.");
			require(dotMat6Count == 1, "Newly added entity in reloaded cluster asset must appear in scene.");
		}
	}

	void testSceneSerializerInMemoryRoundTrip(SceneManager& manager, const std::string& namePrefix)
	{
		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;
		manager.setAssetsPath(ASSETS_PATH);
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-src");
		manager.registerScene<InspectableScene<Scene>>(namePrefix + "-dst");
		manager.loadScene(namePrefix + "-src");

		nlohmann::json savedJson;
		Entity srcBox = INVALID_ENTITY;
		{
			auto& scene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *scene.registry;
			auto& services = *scene.services;

			srcBox = registry.createEntity();
			auto& shape = registry.addComponent<Shape>(srcBox);
			shape.distanceFieldId = static_cast<ShapeId>(DefaultShapes::Box);
			shape.parameters[0] = 3.5f;
			shape.parameters[1] = -2.0f;
			shape.parameters[2] = 1.25f;
			shape.parameters[3] = 0.75f;
			shape.material = 4;
			services.names().setName(srcBox, "UndoBox");
			services.tags().addTag(srcBox, "editable");

			savedJson = SceneSerializer::serializeToJson(scene);
			savedJson.erase("camera");
		}

		manager.loadScene(namePrefix + "-dst");
		{
			auto& dstScene = *static_cast<InspectableScene<Scene>*>(manager.getCurrentScene());
			auto& registry = *dstScene.registry;
			auto& services = *dstScene.services;

			const Entity camEnt = services.render().getCameraEntity();
			registry.getComponent<Transform>(camEnt).position = vec3(42.0f, -17.0f, 20.0f);

			std::unordered_map<Entity, Entity> newToSaved;
			SceneSerializer::loadFromJson(dstScene, savedJson, "", nullptr, false, nullptr, &newToSaved);

			const vec3 camPosAfter = registry.getComponent<Transform>(camEnt).position;
			require(std::abs(camPosAfter.x - 42.0f) < 1e-4f && std::abs(camPosAfter.y + 17.0f) < 1e-4f,
					"loadFromJson(restoreCamera=false) must preserve the editor camera position.");

			const Entity restoredBox = services.names().getEntityByName("UndoBox");
			require(restoredBox != INVALID_ENTITY && registry.hasComponent<Shape>(restoredBox),
					"In-memory loadFromJson must restore entity and Shape component.");
			require(newToSaved.count(restoredBox) == 1 && newToSaved[restoredBox] == srcBox,
					"outSourceEntityIds must map restored entity back to serialized source id.");
			require(services.tags().hasTag(restoredBox, "editable"),
					"In-memory loadFromJson must restore entity tags.");
			const auto& restoredShape = registry.getComponent<Shape>(restoredBox);
			require(std::abs(restoredShape.parameters[0] - 3.5f) < 1e-4f && restoredShape.material == 4,
					"In-memory loadFromJson must restore Shape parameters and material.");
		}
	}
} // namespace

int main()
{
	try
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
		_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
		Logger::s_enableConsoleOutput = false;
		std::cout << "Checking unsupported types.\n" << std::flush;
		testUnsupportedTypes();
		std::cout << "Checking type identity.\n" << std::flush;
		testTypeIdentity();
		std::cout << "Checking deferred destruction.\n" << std::flush;
		testDeferredDestruction();
		std::cout << "Checking damaged snapshots.\n" << std::flush;
		testDamagedSnapshots();
		std::cout << "Checking snapshot determinism.\n" << std::flush;
		testDeterministicSnapshots();
		std::cout << "Checking NodeGraph multi-output and memoization.\n" << std::flush;
		testNodeGraphMultiOutputAndMemoization();
		std::cout << "Checking resource and shape asset path resolution.\n" << std::flush;
		testResourceAndShapeAssetPathResolution();
		std::cout << "Checking TextButton and TextToggle engine components.\n" << std::flush;
		testTextButtonAndToggleComponents();
		std::cout << "Checking builtin rotated and rounded shapes.\n" << std::flush;
		testBuiltinRotatedAndRoundedShapes();
		std::cout << "Checking GPU 2D shape packing parity.\n" << std::flush;
		testGpuShape2DPackingParity();
		auto& manager = SceneManager::getInstance();
		for (bool threaded : {false, true})
		{
			PhysicsSettings settings;
			settings.runSimulationInThread = threaded;
			manager.setPhysicsSettings(settings);
			const std::string suffix = threaded ? "-threaded" : "-single";
			std::cout << "Checking Simulation2D" << suffix << ".\n" << std::flush;
			testSimulation2DPendingAndCollisionFlags(threaded);
			std::cout << "Checking RopeScene" << suffix << ".\n" << std::flush;
			testRopeScene(manager, "test-rope" + suffix);
			std::cout << "Checking TetrominoScene" << suffix << ".\n" << std::flush;
			testTetrominoScene(manager, "test-tetromino" + suffix);
			std::cout << "Checking ThrustScene" << suffix << ".\n" << std::flush;
			testThrustScene(manager, "test-thrust" + suffix);
			std::cout << "Checking PlatformerWorld" << suffix << ".\n" << std::flush;
			testPlatformerWorldClusterFixedBody(manager, "test-platformer" + suffix);
			std::cout << "Checking ClusterOverrides" << suffix << ".\n" << std::flush;
			testClusterReloadPreservesSceneOverrides(manager, "test-cluster-ov" + suffix);
			std::cout << "Checking SceneSerializerJson" << suffix << ".\n" << std::flush;
			testSceneSerializerInMemoryRoundTrip(manager, "test-serializer-json" + suffix);
			std::cout << "Checking MusicScene" << suffix << ".\n" << std::flush;
			testMusicScene(manager, "test-music" + suffix);
			std::cout << "Checking InstrumentScene" << suffix << ".\n" << std::flush;
			testInstrumentScene(manager, "test-instrument" + suffix);
			std::cout << "Checking MenuScene" << suffix << ".\n" << std::flush;
			testMenuScene(manager, "test-menu" + suffix);
		}
		std::cout << "Snapshot regression tests passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
