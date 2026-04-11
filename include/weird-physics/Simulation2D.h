#pragma once

#include <atomic>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <immintrin.h>
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

namespace WeirdEngine
{

	using namespace ECS;

	using SimulationID = std::uint32_t;

	enum class CollisionState
	{
		START,
		CONTINUE,
		END
	};

	struct CollisionEvent
	{
		// CollisionState state;
		SimulationID bodyA;
		SimulationID bodyB;
	};

	struct ShapeCollisionEvent
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
	using CollisionCallbackFn = void (*)(CollisionEvent&, void*);
	using ShapeCollisionCallbackFn = void (*)(ShapeCollisionEvent&, void*);

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
		void removeObject(SimulationID id);
		size_t getSize();

		// Interaction
		void addForce(SimulationID id, const vec2& force);
		void addSpring(SimulationID a, SimulationID b, float stiffness, float distance = 1.0f);
		void addPositionConstraint(SimulationID a, SimulationID b, float distance = 1.0f);
		void addGravitationalConstraint(SimulationID a, SimulationID b, float gravity);
		bool setDistanceConstraintDistance(SimulationID a, SimulationID b, float distance);

		void fix(SimulationID id);
		void unFix(SimulationID id);

		// Retrieve results
		vec2 getPosition(SimulationID id);
		void setPosition(SimulationID id, vec2 pos);
		void updateTransform(Transform& transform, SimulationID id);
		void setMass(SimulationID id, float mass);

		void setSDFs(std::vector<std::shared_ptr<IMathExpression>>& sdfs);

		void updateShape(CustomShape& shape);
		void removeShape(CustomShape& shape);

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
		void checkCollisions();
		void solveCollisionsPositionBased();
		void applyForces();
		void solveConstraints();
		void integrateVelocity(float timeStep);
		void integratePredict(float timeStep);

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

		bool m_isPaused;
		bool m_simulating;
		double m_simulationDelay;
		std::atomic<double> m_simulationTime{0.0};

		bool m_useSimdOperations;

		vec2* m_positions;
		vec2* m_positionsRead;
		;
		vec2* m_positionsAux;
		;

		vec2* m_previousPositions;
		vec2* m_velocities;
		vec2* m_forces;

		bool m_externalForcesSinceLastUpdate;
		vec2* m_externalForces;

		size_t m_maxSize;
		size_t m_size;

		float* m_mass;
		float* m_invMass;

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
		std::vector<ShapeCollisionEvent> m_collisionQueue;

		float map(vec2 p);
		float map(vec2 p, int& closestShape);

		// Collision
		CollisionDetectionMethod m_collisionDetectionMethod;

		std::vector<Collision> m_collisions;
		std::vector<int> m_treeIDs;
		std::unordered_map<int, SimulationID> m_treeIdToSimulationID;

		// Constraints
		std::vector<SimulationID> m_fixedObjects;
		std::vector<DistanceConstraint> m_distanceConstraints;
		std::vector<GravitationalConstraint> m_gravitationalConstraints;

		std::thread m_simulationThread;
		void runSimulationThread();

		// Extra
		bool m_attracttionEnabled = false;
		bool m_repulsionEnabled = false;
		bool m_liftEnabled = false;

		std::mutex m_fixMutex;
		std::mutex m_externalForcesMutex;
		std::recursive_mutex m_objectMutex;

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