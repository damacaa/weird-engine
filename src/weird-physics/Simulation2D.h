#pragma once
#include "../weird-renderer/Shape.h" // TODO: replace with components
#include "../weird-engine/ecs/Entity.h"
#include "../weird-engine/ecs/Components/Transform.h"
#include <vector>
#include <thread>
#include <unordered_set>

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


	void checkCollisions();
	void step(float timeStep);
	//void setSize(unsigned int size);
	SimulationID generateSimulationID();
	size_t getSize();

	// Add external forces
	void shake(float f);
	void push(vec2 v);

	// Retrieve results
	vec2 getPosition(SimulationID entity);
	void setPosition(SimulationID entity, vec2 pos);
	void updateTransform(Transform& transform, SimulationID entity);

private:

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

	bool m_useSimdOperations;

	vec2* m_positions;
	vec2* m_velocities;
	vec2* m_forces;

	size_t m_maxSize;
	size_t m_size;

	float* m_mass;
	float* m_invMass;

	const float m_diameter = 1.0f;
	const float m_diameterSquared = m_diameter * m_diameter;
	const float m_radious = 0.5f * m_diameter;

	const float m_push = 100.0f;
	const float m_damping = 10.0f;

	const float m_gravity = -10.0f;

	CollisionDetectionMethod m_collisionDetectionMethod;
	//std::vector<Collision> m_collisions;
	std::unordered_set<Collision, CollisionHash> m_collisions;

	std::thread m_simulationThread;
	void runSimulationThread();
};

