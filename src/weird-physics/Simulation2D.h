#pragma once
#include "../weird-renderer/Shape.h" // TODO: replace with components
#include "../weird-engine/ecs/Entity.h"
#include "../weird-engine/ecs/Components/Transform.h"
#include <vector>
#include <thread>
#include <unordered_set>

#include "CollisionDetection/UniformGrid2D.h"

using SimulationID = std::uint32_t;

using vec2 = glm::vec2;

class Simulation2D
{


public:
	Simulation2D(size_t size);
	~Simulation2D();

	// Manage simulation
	void startSimulationThread();
	void stopSimulationThread();


	void update(double delta);



	//void setSize(unsigned int size);
	SimulationID generateSimulationID();
	size_t getSize();

	// Add external forces
	void shake(float f);
	void push(vec2 v);
	void addForce(SimulationID id, vec2 force);

	// Retrieve results
	vec2 getPosition(SimulationID entity);
	void setPosition(SimulationID entity, vec2 pos);
	void updateTransform(Transform& transform, SimulationID entity);

private:

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

	struct CollisionHash {
		std::size_t operator()(const Collision& s) const {
			bool flip = s.A < s.B;
			int first = flip ? s.B : s.A;
			int last = flip ? s.A : s.B;
			return std::hash<int>()(first) ^ std::hash<int>()(last);
		}
	};

	enum CollisionDetectionMethod {
		None,
		MethodNaive,
		MethodUniformGrid
	};

	bool m_simulating;
	double m_simulationDelay;
	double m_simulationTime;

	bool m_useSimdOperations;

	vec2* m_positions;
	vec2* m_velocities;
	vec2* m_forces;

	bool m_externalForcesSinceLastUpdate;
	vec2* m_externalForces;

	size_t m_maxSize;
	size_t m_size;

	float* m_mass;
	float* m_invMass;

	const float m_diameter = 1.0f;
	const float m_diameterSquared = m_diameter * m_diameter;
	const float m_radious = 0.5f * m_diameter;

	const float m_push;
	const float m_damping;

	const float m_gravity;

	CollisionDetectionMethod m_collisionDetectionMethod;
	//std::vector<Collision> m_collisions;
	std::unordered_set<Collision, CollisionHash> m_collisions;



	UniformGrid2D grid;

	std::thread m_simulationThread;
	void runSimulationThread();
};

