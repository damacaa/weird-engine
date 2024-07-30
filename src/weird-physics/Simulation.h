#pragma once
#include "../weird-renderer/Shape.h" // TODO: replace with components
#include "../weird-engine/ecs/Entity.h"
#include "../weird-engine/ecs/Components/Transform.h"
#include <vector>

using SimulationID = std::uint32_t;

using glm::vec3;

class Simulation
{


public:
	Simulation(size_t size);
	~Simulation();

	// Manage simulation
	void update(double delta);
	void checkCollisions();
	void step(float timeStep);
	//void setSize(unsigned int size);
	SimulationID generateSimulationID();
	size_t getSize();

	// Add external forces
	void shake(float f);
	void push(vec3 v);

	// Retrieve results
	vec3 getPosition(SimulationID entity);
	void setPosition(SimulationID entity, vec3 pos);
	void updateTransform(Transform& transform, SimulationID entity);

private:

	struct Collision
	{
	public:
		Collision() {
			A = -1;
			B = -1;
			AB = vec3();
		}


		Collision(int a, int b, vec3 ab) {
			A = a;
			B = b;
			AB = ab;
		}

		int A;
		int B;
		vec3 AB;
	};

	enum CollisionDetectionMethod {
		None,
		NaiveMethod,
		SpatialHashMethod,
		OctreeMethod
	};


	double m_simulationDelay;

	vec3* m_positions;
	vec3* m_velocities;
	vec3* m_forces;

	size_t m_maxSize;
	size_t m_size;

	const float m_mass = 100.0f;
	const float m_invMass = 1.0f / m_mass;

	const float m_diameter = 1.0f;
	const float m_diameterSquared = m_diameter * m_diameter;
	const float m_radious = 0.5f * m_diameter;

	const float m_push = 100.0f;
	const float m_damping = 1.0f;

	const float m_gravity = -9.81f;

	CollisionDetectionMethod m_collisionDetectionMethod;
	std::vector<Collision> m_collisions;

};

