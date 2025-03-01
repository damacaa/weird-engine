#pragma once

#include<glm/glm.hpp>

#include "../weird-engine/ecs/Entity.h"
// #include "../weird-engine/ecs/ComponentArray.h"
#include "../weird-engine/ecs/Components/Transform.h"

#include "../weird-engine/ecs/Components/CustomShape.h"

#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include <bitset>

#include "CollisionDetection/UniformGrid2D.h"
#include "CollisionDetection/DynamicAABBTree2D.h"

#include "CollisionDetection/SpatialHash.h"
#include "../weird-engine/Input.h"
#include "CollisionDetection/Octree.h"

#include <chrono>
#include <immintrin.h>
#include <mutex>
#include <set>
#include <glm/gtx/norm.hpp>

#include "../weird-engine/math/MathExpressions.h"

namespace WeirdEngine
{

	using namespace ECS;

	using SimulationID = std::uint32_t;

	using vec2 = glm::vec2;


	class CustomBitset
	{
	public:
		CustomBitset(size_t size) : bits((size + 63) / 64, 0), size(size) {}

		void set(SimulationID pos)
		{
			if (pos < size) {
				bits[pos / 64] |= (1ULL << (pos % 64));
			}
		}

		void clear(SimulationID pos)
		{
			if (pos < size) {
				bits[pos / 64] &= ~(1ULL << (pos % 64));
			}
		}

		bool test(SimulationID pos) const
		{
			if (pos < size)
			{
				return bits[pos / 64] & (1ULL << (pos % 64));
			}
			return false;
		}

	private:
		std::vector<uint64_t> bits;
		size_t size;
	};




	class Simulation2D
	{


	public:
		Simulation2D(size_t size);
		~Simulation2D();

		// Manage simulation
		void pause();
		void resume();
		bool isPaused();

		void startSimulationThread();
		void stopSimulationThread();


		void update(double delta);

		double getSimulationTime();

		// void setSize(unsigned int size);
		SimulationID generateSimulationID();
		void removeObject(SimulationID id);
		size_t getSize();

		// Interaction
		void addForce(SimulationID id, vec2 force);
		void addSpring(SimulationID a, SimulationID b, float stiffness, float distance = 1.0f, float daping = 1000.0f);
		void addPositionConstraint(SimulationID a, SimulationID b, float distance = 1.0f);
		void addGravitationalConstraint(SimulationID a, SimulationID b, float gravity);

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

		void setGravity(float gravity) { m_gravity = gravity; }
		void setDamping(float damping) { m_damping = damping; }

	private:
		void process();
		void checkCollisions();
		void solveCollisionsPositionBased();
		void applyForces();
		void solveConstraints();
		void step(float timeStep);


		struct Collision
		{
		public:
			Collision() {
				A = -1;
				B = -1;
				AB = vec2();
			}


			Collision(int a, int b, vec2 ab) {
				A = a;
				B = b;
				AB = ab;
			}


			bool operator==(const Collision& other) const {
				return (A == other.A && B == other.B) || (A == other.B && B == other.A);
			}

			int A;
			int B;
			vec2 AB;
		};

		struct Spring
		{
		public:
			Spring()
			{
				A = -1;
				B = -1;
				K = 0;
			}

			Spring(int a, int b, float k, float distance, float damping)
			{
				A = a;
				B = b;
				K = k;
				Distance = distance;
				Damping = damping;
			}

			int A;
			int B;
			float K;
			float Distance;
			float Damping;
		};

		struct DistanceConstraint
		{
		public:
			DistanceConstraint()
			{
				A = -1;
				B = -1;
				Distance = 1.0f;
			}


			DistanceConstraint(int a, int b, float distance)
			{
				A = a;
				B = b;
				Distance = distance;
			}

			int A;
			int B;
			float Distance;
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

		struct CollisionHash {
			std::size_t operator()(const Collision& s) const {
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
			float parameters[11];

			DistanceFieldObject2D(Entity owner, uint16_t id, float* params) : distanceFieldId(id), owner(owner)
			{
				std::copy(params, params + 8, parameters); // Copy params into parameters
			}
		};



		bool m_isPaused;
		bool m_simulating;
		double m_simulationDelay;
		double m_simulationTime;

		bool m_useSimdOperations;

		vec2* m_positions;
		vec2* m_previousPositions;
		vec2* m_velocities;
		vec2* m_forces;

		bool m_externalForcesSinceLastUpdate;
		vec2* m_externalForces;

		size_t m_maxSize;
		size_t m_size;
		size_t m_lastIdGiven;

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

		float map(vec2 p);

		// Collision
		CollisionDetectionMethod m_collisionDetectionMethod;

		std::vector<Collision> m_collisions;
		DynamicAABBTree m_tree;
		std::vector<int> m_treeIDs;
		std::unordered_map<int, SimulationID> m_treeIdToSimulationID;

		// Constraints
		std::vector<SimulationID> m_fixedObjects;
		std::vector<Spring> m_springs;
		std::vector<DistanceConstraint> m_distanceConstraints;
		std::vector<GravitationalConstraint> m_gravitationalConstraints;

		std::thread m_simulationThread;
		void runSimulationThread();

		// Extra
		bool m_attracttionEnabled = false;
		bool m_repulsionEnabled = false;
		bool m_liftEnabled = false;
	};

}