#pragma once

#include <atomic>
#include <bitset>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../weird-engine/components/Transform.h"
#include "../weird-renderer/components/CustomShape.h"
#include "weird-engine/ecs/Entity.h"
#include "weird-engine/Input.h"
#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/vec.h"

#include "PhysicsSettings.h"
#include "weird-physics/BodyUserData.h"
#include "weird-physics/SimulationID.h"

namespace WeirdEngine
{

	using namespace ECS;

	enum class PhysicsCommandType
	{
		SetVelocity,
		SetPosition,
		Fix,
		UnFix,
		SetMass,
		ActivatePending,
		AddImpulse,
		Action,
		EnableCollision,
		DisableCollision
	};

	struct PhysicsCommand
	{
		PhysicsCommandType type = PhysicsCommandType::SetVelocity;
		SimulationID id = 0;
		vec2 vectorData = vec2(0.0f);
		float floatData = 0.0f;
		std::function<void()> action;
	};

	enum class CollisionState
	{
		START,
		CONTINUE,
		END
	};

	struct PhysicsCollisionEvent
	{
		SimulationID bodyA = 0;
		SimulationID bodyB = 0;
		vec2 position = vec2(0.0f);
		vec2 normal = vec2(0.0f);
		vec2 relativeVelocity = vec2(0.0f);
		float impulse = 0.0f;
		bool ignoreCollision = false;
	};

	struct PhysicsShapeCollisionEvent
	{
		CollisionState state = CollisionState::END;
		SimulationID body = 0;
		ShapeId shape = 0;
		float penetration = 0.0f;
		float friction = 0.0f;
		float absortion = 0.0f;
		vec2 position = vec2(0.0f);
		vec2 velocity = vec2(0.0f);
		vec2 normal = vec2(0.0f);
	};

	// Define the function pointer type and include a user data pointer
	using StepCallbackFn = void (*)(void*);

	// Define the function pointer type and include a user data pointer
	using CollisionCallbackFn = void (*)(PhysicsCollisionEvent&, void*);
	using ShapeCollisionCallbackFn = void (*)(PhysicsShapeCollisionEvent&, void*);

	struct SpatialGridSnapshot
	{
		std::vector<int> head;
		std::vector<int> next;
		std::vector<vec2> positions;
		float invCellSize = 0.0f;
		float radious = 0.0f;
	};

	class Simulation2D
	{

	public:
		Simulation2D(size_t size, const PhysicsSettings& settings);
		~Simulation2D();

		// Manage simulation
		void pause();
		void resume();
		bool isPaused();

		void startSimulationThread();
		void stopSimulationThread();

		void update(double delta);

		double getSimulationTime();
		double getDeltaTime()
		{
			return m_fixedDeltaTime;
		}

		// void setSize(unsigned int size);
		// Main-thread lifecycle: creation reserves an ID and queues initialization.
		// Activation publishes the completed batch. Removal waits for a physics
		// boundary so the caller can immediately apply the last-slot ID remapping.
		SimulationID generateSimulationID();
		void activatePendingBodies();
		void beginCommandBatch();
		void endCommandBatch();
		void removeObject(SimulationID id);
		size_t getSize();

		// Interaction
		// One-shot kick applied to a body. With massIndependent = false the
		// impulse is a force scaled by the simulation frequency; with
		// massIndependent = true it is applied as a direct velocity change
		// (the parameter is the desired delta-v in m/s, regardless of mass).
		void addImpulseForce(SimulationID id, const vec2& impulse, bool massIndependent = false);
		void setContinuousForce(SimulationID id, const vec2& force, bool massIndependent = false);
		void swapContinuousForces();
		void addSpring(SimulationID a, SimulationID b, float stiffness, float distance = 1.0f);
		void addPositionConstraint(SimulationID a, SimulationID b, float distance = 1.0f);
		void addGravitationalConstraint(SimulationID a, SimulationID b, float gravity);
		bool setDistanceConstraintDistance(SimulationID a, SimulationID b, float distance);
		bool removeDistanceConstraint(SimulationID a, SimulationID b);

		void fix(SimulationID id);
		void unFix(SimulationID id);
		bool isFixed(SimulationID id);

		void enableCollision(SimulationID id);
		void disableCollision(SimulationID id);
		void setCollisionEnabled(SimulationID id, bool enabled);
		bool isCollisionEnabled(SimulationID id);

		// Performance Stats
		struct PerformanceStats
		{
			double timePerStepMs = 0.0;
			double simulationRatio = 0.0;
			double broadPhaseMs = 0.0;
			double narrowPhaseMs = 0.0;
			double shapeEvaluationMs = 0.0;
			double integrationMs = 0.0;
			double collisionEventsMs = 0.0;
		};

		PerformanceStats getPerformanceStats() const
		{
			std::lock_guard<std::mutex> lock(m_statsMutex);
			return m_stats;
		}

		// Retrieve published results. Safe from any thread, including physics
		// callbacks (they just pay a per-call lock). For reading many bodies
		// at once on the main thread, copy them into a ReadBufferSnapshot via
		// copyReadBuffers() instead.
		vec2 getPosition(SimulationID id);
		void setPosition(SimulationID id, vec2 pos);
		vec2 getVelocity(SimulationID id);
		void setVelocity(SimulationID id, vec2 vel);
		void setMass(SimulationID id, float mass);

		// Published physics state copied out under a brief lock. Fill it once
		// per frame with copyReadBuffers(), then iterate the ECS without
		// holding the simulation mutex.
		struct ReadBufferSnapshot
		{
			std::vector<vec2> positions;
			std::vector<vec2> velocities;
		};

		// Copies the published positions/velocities into the snapshot under a
		// short lock. The snapshot's buffers grow as needed but keep their
		// capacity across calls. Main thread only; must NOT be called from
		// physics execution (WEIRD_ASSERT enforces this in debug builds).
		void copyReadBuffers(ReadBufferSnapshot& snapshot);

		// Current working physics state. PHYSICS EXECUTION ONLY: call these
		// from onPhysicsStep/onCollision/onShapeCollision callbacks, never
		// from the main thread (WEIRD_ASSERT enforces this in debug builds).
		vec2 getPhysicsPosition(SimulationID id) const;
		vec2 getPhysicsVelocity(SimulationID id) const;

		// True while inside a physics step (physics thread in threaded mode,
		// main thread in single-threaded mode).
		static bool isPhysicsExecutionContext();

		void setSDFs(std::vector<std::shared_ptr<IMathExpression>>& sdfs);

		void setAudioVolume(float volume)
		{
			m_audioVolume.store(volume, std::memory_order_relaxed);
		}

		float getAudioVolume() const
		{
			return m_audioVolume.load(std::memory_order_relaxed);
		}

		std::shared_ptr<SpatialGridSnapshot> getSpatialGridSnapshot()
		{
			std::lock_guard<std::mutex> lock(m_spatialGridSnapshotMutex);
			return m_spatialGridSnapshot;
		}

		void updateShape(Entity owner, CustomShape& shape);
		void removeShape(Entity owner, CustomShape& shape);

		SimulationID raycast(vec2 pos);
		float raymarch(vec2 pos, vec2 direction, const float FAR = 100.0f);
		float raymarch(vec2 pos, vec2 direction, const float FAR, int& closestShape);

		void setGravity(float gravity)
		{
			m_gravity = gravity;
		}
		float getGravity() const
		{
			return m_gravity;
		}
		void setDamping(float damping)
		{
			m_damping = damping;
		}
		float getDamping() const
		{
			return m_damping;
		}

		// Per-body user data, keyed by SimulationID (entity-free: the ECS maps
		// simulation IDs back to entities via the RigidBody2D component array).
		// Takes ownership via unique_ptr: the simulation deletes the data when
		// the body is removed (removeObject) and when the simulation is
		// destroyed. Do not retain the pointer after the call; read or modify
		// it through getUserData()/getUserDataAs<T>() instead. setUserData() is
		// main-thread only; getUserData()/getUserDataAs<T>()/forEachUserData()
		// may be called from physics callbacks. forEachUserData holds the data
		// lock throughout its callback. Raw pointers must not be retained across
		// replacement/removal; use forEachUserData for concurrent main-thread edits.
		void setUserData(SimulationID id, std::unique_ptr<BodyUserData> data);
		BodyUserData* getUserData(SimulationID id);

		// Type-checked cast: returns nullptr unless the attached data exists
		// and its `type` matches T::TYPE.
		template <typename T> T* getUserDataAs(SimulationID id)
		{
			BodyUserData* data = getUserData(id);
			if (!data || data->type != T::TYPE)
				return nullptr;
			return static_cast<T*>(data);
		}

		// Calls fn(SimulationID, BodyUserData&) for every active body that has
		// user data attached. Protected by m_userDataMutex. Main-thread callbacks
		// must not call synchronous physics operations (removal, constraint queries
		// or edits returning bool): those wait for the worker, which needs this lock.
		template <typename Fn> void forEachUserData(Fn&& fn)
		{
			std::lock_guard<std::recursive_mutex> lock(m_userDataMutex);
			for (SimulationID id = 0; id < m_size; ++id)
			{
				if (m_bodyActive[id] && m_userData[id])
					fn(id, *m_userData[id]);
			}
		}

		// Constraint structs (public for serialization)
		struct DistanceConstraint
		{
		public:
			SimulationID A = INVALID_SIMULATION_ID;
			SimulationID B = INVALID_SIMULATION_ID;
			float Distance = 1.0f;
			float K = 0.0f;

			DistanceConstraint() = default;

			DistanceConstraint(SimulationID a, SimulationID b, float distance, float k = 1.0f)
				: A(a)
				, B(b)
				, Distance(distance)
				, K(k)
			{
			}
		};

		struct GravitationalConstraint
		{
		public:
			SimulationID A = INVALID_SIMULATION_ID;
			SimulationID B = INVALID_SIMULATION_ID;
			float g = 1.0f;

			GravitationalConstraint() = default;

			GravitationalConstraint(SimulationID a, SimulationID b, float gravity)
				: A(a)
				, B(b)
				, g(gravity)
			{
			}
		};

		// Serialization support: read constraint data
		std::vector<DistanceConstraint> getDistanceConstraints() const
		{
			std::vector<DistanceConstraint> result;
			const_cast<Simulation2D*>(this)->executeSynchronous([&] { result = m_distanceConstraints; });
			return result;
		}
		std::vector<GravitationalConstraint> getGravitationalConstraints() const
		{
			std::vector<GravitationalConstraint> result;
			const_cast<Simulation2D*>(this)->executeSynchronous([&] { result = m_gravitationalConstraints; });
			return result;
		}
		std::vector<SimulationID> getFixedObjects() const
		{
			std::vector<SimulationID> result;
			const_cast<Simulation2D*>(this)->executeSynchronous([&] { result = m_fixedObjects; });
			return result;
		}

		// Serialization support: load raw constraint (bypasses stiffness conversion)
		void addRawDistanceConstraint(SimulationID a, SimulationID b, float distance, float k)
		{
			if (a == b)
				return;
			enqueueAction([=, this] { m_distanceConstraints.emplace_back(a, b, distance, k); });
		}

	private:
		void process();
		void processCommands();
		void removeBodyAtBoundary(SimulationID id);
		void enqueueCommand(PhysicsCommand command);
		void enqueueAction(std::function<void()> action);
		void executeSynchronous(std::function<void()> action);
		void checkCollisions(double& broadPhaseMs, double& narrowPhaseMs, double& shapeEvaluationMs);
		void solveCollisionsPositionBased();
		void applyForces();
		void solveConstraints();
		void integrateVelocity(float timeStep);
		void integratePredict(float timeStep);

		void internalUpdateShape(Entity owner, CustomShape& shape);
		void internalRemoveShape(Entity owner, CustomShape& shape);

		struct Collision
		{
		public:
			SimulationID A = 0;
			SimulationID B = 0;
			vec2 AB = vec2(0.0f);

			Collision() = default;

			Collision(SimulationID a, SimulationID b, vec2 ab)
				: A(a)
				, B(b)
				, AB(ab)
			{
			}

			bool operator==(const Collision& other) const
			{
				return (A == other.A && B == other.B) || (A == other.B && B == other.A);
			}
		};

		struct CollisionHash
		{
			std::size_t operator()(const Collision& s) const
			{
				bool flip = s.A < s.B;
				SimulationID first = flip ? s.B : s.A;
				SimulationID last = flip ? s.A : s.B;
				return std::hash<SimulationID>()(first) ^ (std::hash<SimulationID>()(last) << 1);
			}
		};

		enum CollisionDetectionMethod
		{
			None,
			MethodNaive,
			MethodTree
		};

		struct DistanceFieldObject2D
		{
			Entity owner = INVALID_ENTITY;
			uint16_t distanceFieldId = 0;
			CombinationType combinationId = CombinationType::Addition;
			uint16_t groupId = 0;
			float parameters[12] = {0.0f};
			float smoothRadius = 1.0f;

			DistanceFieldObject2D() = default;

			DistanceFieldObject2D(Entity owner, uint16_t id, CombinationType combinationId, uint16_t groupId,
								  float* params, float smoothRadius = 1.0f)
				: owner(owner)
				, distanceFieldId(id)
				, combinationId(combinationId)
				, groupId(groupId)
				, smoothRadius(smoothRadius)
			{
				std::copy(params, params + 8, parameters); // Copy params into parameters
			}
		};

		int m_substeps; // TODO: implement substeps
		float m_simulationFrequency;
		double m_fixedDeltaTime;
		float m_fixedDeltaTimeF;
		int m_relaxationSteps;

		std::atomic<bool> m_isPaused{false};
		std::atomic<bool> m_simulating{false};
		std::atomic<double> m_simulationDelay{0.0};
		std::atomic<double> m_simulationTime{0.0};

		bool m_useSimdOperations;

		vec2* m_positions;
		vec2* m_positionsRead;
		;
		vec2* m_positionsAux;
		;

		vec2* m_previousPositions;
		vec2* m_velocities;
		vec2* m_velocitiesRead;
		vec2* m_velocitiesAux;
		vec2* m_forces;

		bool m_impulsesSinceLastUpdate;
		vec2* m_impulses;
		vec2* m_continuousForcesRead;
		vec2* m_continuousForcesWrite;

		size_t m_maxSize;
		// Physics owns initialized slots; the game thread reserves IDs.
		size_t m_size;
		std::atomic<size_t> m_allocated;
		std::atomic<size_t> m_activeSize{0};
		std::vector<uint8_t> m_bodyActive;
		std::vector<uint8_t> m_collisionEnabled;

		float* m_mass;
		float* m_invMass;

		// Per-body user data, parallel to the body arrays. Swapped in
		// removeObject() so the data follows the body through renumbering.
		BodyUserData** m_userData;

		const float m_diameter;
		const float m_diameterSquared;
		const float m_radious;

		float m_push;
		float m_damping;

		float m_gravity;

		// Shapes
		std::unordered_map<Entity, uint16_t> m_entityToObjectsIdx;
		std::shared_ptr<std::vector<std::shared_ptr<IMathExpression>>> m_sdfs;
		std::shared_ptr<std::vector<std::shared_ptr<IMathExpression>>> m_sdfsSnapshot; // sim-thread-only copy
		std::vector<DistanceFieldObject2D> m_objects;

		std::vector<uint8_t> m_collisionMap;
		std::vector<PhysicsShapeCollisionEvent> m_collisionQueue;

		float map(vec2 p);
		float map(vec2 p, int& closestShape);

		// Collision
		CollisionDetectionMethod m_collisionDetectionMethod;

		std::vector<int> m_head;
		std::vector<int> m_next;

		std::vector<Collision> m_collisions;
		std::vector<int> m_treeIDs;
		std::unordered_map<int, SimulationID> m_treeIdToSimulationID;

		// Constraints
		std::vector<SimulationID> m_fixedObjects;
		std::vector<DistanceConstraint> m_distanceConstraints;
		std::vector<GravitationalConstraint> m_gravitationalConstraints;

		std::thread m_simulationThread;
		std::thread::id m_physicsThreadId;
		void runSimulationThread();

		// Extra
		bool m_attracttionEnabled = false;
		bool m_repulsionEnabled = false;
		bool m_liftEnabled = false;

		std::mutex m_spatialGridSnapshotMutex;
		std::shared_ptr<SpatialGridSnapshot> m_spatialGridSnapshot;

		std::mutex m_fixMutex;
		std::mutex m_externalForcesMutex;
		std::mutex m_structuralMutex;
		mutable std::recursive_mutex m_userDataMutex;
		std::mutex m_readMutex;
		std::mutex m_commandMutex;
		std::condition_variable m_commandReady;
		bool m_batchingCommands = false;
		std::vector<PhysicsCommand> m_pendingCommands;
		std::vector<PhysicsCommand> m_internalCommands;

		struct ShapeUpdateCommand
		{
			bool isRemove = false;
			Entity owner = INVALID_ENTITY;
			CustomShape shape;
		};
		std::mutex m_shapeUpdateMutex;
		std::vector<ShapeUpdateCommand> m_pendingShapeUpdates;

		mutable std::mutex m_statsMutex;
		PerformanceStats m_stats;

		std::atomic<float> m_audioVolume{0.0f};

	private:
		StepCallbackFn m_stepCallback = nullptr;
		CollisionCallbackFn m_collisionCallback = nullptr;
		ShapeCollisionCallbackFn m_shapeCollisionCallback = nullptr;
		void* m_callbackUserData = nullptr;

	public:
		void setStepCallback(StepCallbackFn callback, void* userData)
		{
			m_stepCallback = callback;
			m_callbackUserData = userData;
		}

		void setCollisionCallback(CollisionCallbackFn callback, void* userData)
		{
			m_collisionCallback = callback;
			m_callbackUserData = userData;
		}

		void setShapeCollisionCallback(ShapeCollisionCallbackFn callback, void* userData)
		{
			m_shapeCollisionCallback = callback;
			m_callbackUserData = userData;
		}
	};

} // namespace WeirdEngine
