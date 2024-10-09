#pragma once
#include "../weird-renderer/Shape.h" // TODO: replace with components
#include "../weird-engine/ecs/Entity.h"
#include "../weird-engine/ecs/Components/Transform.h"
#include <vector>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <cstdint>
#include <bitset>

#include "CollisionDetection/UniformGrid2D.h"
#include "CollisionDetection/DynamicAABBTree2D.h"

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
	void startSimulationThread();
	void stopSimulationThread();


	void update(double delta);

	double getSimulationTime();

	//void setSize(unsigned int size);
	SimulationID generateSimulationID();
	size_t getSize();

	// Interaction
	void addForce(SimulationID id, vec2 force);
	void addSpring(SimulationID a, SimulationID b, float stiffness);


	// Retrieve results
	vec2 getPosition(SimulationID entity);
	void setPosition(SimulationID entity, vec2 pos);
	void updateTransform(Transform& transform, SimulationID entity);

private:

	void process();
	void checkCollisions();
	void solveCollisionsPositionBased();
	void applyForces();
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


		Spring(int a, int b, float k)
		{
			A = a;
			B = b;
			K = k;
		}

		int A;
		int B;
		float K;
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

	const float m_push;
	const float m_damping;

	const float m_gravity;

	CollisionDetectionMethod m_collisionDetectionMethod;

	std::vector<Collision> m_collisions;
	DynamicAABBTree m_tree;
	std::vector<int> m_treeIDs;
	std::unordered_map<int, SimulationID> m_treeIdToSimulationID;


	std::vector<Spring> m_springs;

	std::thread m_simulationThread;
	void runSimulationThread();
};

