#include "ImageScene.h"
#include "TestHarness.h"
#include "weird-physics/components/Spring.h"

#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::require;

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
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(argc, argv, "WeirdSnapshotTests",
								{
									{"UnsupportedTypes", testUnsupportedTypes},
									{"TypeIdentity", testTypeIdentity},
									{"DeferredDestruction", testDeferredDestruction},
									{"DamagedSnapshots", testDamagedSnapshots},
									{"DeterministicSnapshots", testDeterministicSnapshots},
								});
}
