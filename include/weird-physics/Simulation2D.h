#pragma once

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
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

namespace WeirdEngine
{

	using namespace ECS;

	using SimulationID = std::uint32_t;

	enum class PhysicsCommandType
	{
		SetVelocity,
		SetPosition,
		Fix,
		UnFix,
		SetMass,
		ActivatePending,
		AddImpulse
	};

	struct PhysicsCommand
	{
		PhysicsCommandType type;
		SimulationID id;
		vec2 vectorData;
		float floatData;
	};

	enum class CollisionState
	{
		START,
		CONTINUE,
		END
	};

	struct PhysicsCollisionEvent
	{
		// CollisionState state;
		SimulationID bodyA;
		SimulationID bodyB;
	};

	struct PhysicsShapeCollisionEvent
	{
		CollisionState state;
		SimulationID body;
		ShapeId shape;
		float penetration;
		float friction;
		float absortion;
		vec2 position;
		vec2 velocity;
		vec2 normal;
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
		float invCellSize;
		float radious;
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
		SimulationID generateSimulationID();
		void activatePendingBodies();
		void removeObject(SimulationID id);
		size_t getSize();

		// Interaction
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
		void setDamping(float damping)
		{
			m_damping = damping;
		}

		// Per-body user data, keyed by SimulationID (entity-free: the ECS maps
		// simulation IDs back to entities via the RigidBody2D component array).
		// Must be heap-allocated: the simulation owns the pointer and deletes
		// it when the body is removed (removeObject) and when the simulation
		// is destroyed. setUserData() is main-thread only; getUserData()/
		// getUserDataAs()/forEachUserData() are safe from the physics
		// callbacks without locks.
		void setUserData(SimulationID id, BodyUserData* data);
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
		// user data attached. Lock-free from physics callbacks (the step
		// already holds the structural mutex); serialized on the main thread.
		template <typename Fn> void forEachUserData(Fn&& fn)
		{
			if (isPhysicsExecutionContext())
			{
				for (SimulationID id = 0; id < m_size; ++id)
				{
					if (m_userData[id])
						fn(id, *m_userData[id]);
				}
				return;
			}

			std::lock_guard<std::mutex> lock(m_structuralMutex);
			for (SimulationID id = 0; id < m_size; ++id)
			{
				if (m_userData[id])
					fn(id, *m_userData[id]);
			}
		}

		// Constraint structs (public for serialization)
		struct DistanceConstraint
		{
		public:
			DistanceConstraint()
			{
				A = -1;
				B = -1;
				Distance = 1.0f;
				K = 0.0f;
			}

			DistanceConstraint(int a, int b, float distance, float k = 1.0f)
			{
				A = a;
				B = b;
				Distance = distance;
				K = k;
			}

			int A;
			int B;
			float Distance;
			float K;
		};

		struct GravitationalConstraint
		{
		public:
			GravitationalConstraint()
			{
				A = -1;
				B = -1;
				g = 1.0f;
			}

			GravitationalConstraint(int a, int b, float gravity)
			{
				A = a;
				B = b;
				g = gravity;
			}

			int A;
			int B;
			float g;
		};

		// Serialization support: read constraint data
		const std::vector<DistanceConstraint>& getDistanceConstraints() const
		{
			return m_distanceConstraints;
		}
		const std::vector<GravitationalConstraint>& getGravitationalConstraints() const
		{
			return m_gravitationalConstraints;
		}
		const std::vector<SimulationID>& getFixedObjects() const
		{
			return m_fixedObjects;
		}

		// Serialization support: load raw constraint (bypasses stiffness conversion)
		void addRawDistanceConstraint(int a, int b, float distance, float k)
		{
			if (a == b)
				return;
			m_distanceConstraints.emplace_back(a, b, distance, k);
		}

	private:
		void process();
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
			Collision()
			{
				A = -1;
				B = -1;
				AB = vec2();
			}

			Collision(SimulationID a, SimulationID b, vec2 ab)
			{
				A = a;
				B = b;
				AB = ab;
			}

			bool operator==(const Collision& other) const
			{
				return (A == other.A && B == other.B) || (A == other.B && B == other.A);
			}

			SimulationID A;
			SimulationID B;
			vec2 AB;
		};

		struct CollisionHash
		{
			std::size_t operator()(const Collision& s) const
			{
				bool flip = s.A < s.B;
				int first = flip ? s.B : s.A;
				int last = flip ? s.A : s.B;
				return std::hash<int>()(first) ^ std::hash<int>()(last);
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
			Entity owner;
			uint16_t distanceFieldId;
			CombinationType combinationId;
			uint16_t groupId;
			float parameters[11];

			DistanceFieldObject2D(Entity owner, uint16_t id, CombinationType combinationId, uint16_t groupId,
								  float* params)
				: distanceFieldId(id)
				, combinationId(combinationId)
				, groupId(groupId)
				, owner(owner)
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
		size_t m_size;
		size_t m_allocated;

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
		std::mutex m_readMutex;
		std::mutex m_commandMutex;
		std::vector<PhysicsCommand> m_pendingCommands;
		std::vector<PhysicsCommand> m_internalCommands;

		struct ShapeUpdateCommand
		{
			bool isRemove;
			Entity owner;
			CustomShape shape;
		};
		std::mutex m_shapeUpdateMutex;
		std::vector<ShapeUpdateCommand> m_pendingShapeUpdates;

		mutable std::mutex m_statsMutex;
		PerformanceStats m_stats;

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