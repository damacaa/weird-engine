#pragma once
#include "Simulation2D.h"
#include "CollisionDetection/SpatialHash.h"
#include "../weird-engine/Input.h"
#include "CollisionDetection/Octree.h"

#include <chrono>
#include <immintrin.h>
#include <mutex>
#include <set>
#include <glm/gtx/norm.hpp>

#define MEASURE_PERFORMANCE false			


using namespace std::chrono;

constexpr float SIMULATION_FREQUENCY = 500;
constexpr double FIXED_DELTA_TIME = 1 / SIMULATION_FREQUENCY;

constexpr size_t MAX_STEPS = 10;

std::mutex g_simulationTimeMutex;
std::mutex g_externalForcesMutex;
std::mutex g_collisionTreeUpdateMutex;


Simulation2D::Simulation2D(size_t size) :
	m_positions(new vec2[size]),
	m_previousPositions(new vec2[size]),
	m_velocities(new vec2[size]),
	m_forces(new vec2[size]),
	m_externalForcesSinceLastUpdate(false),
	m_externalForces(new vec2[size]),
	m_mass(new float[size]),
	m_invMass(new float[size]),
	m_maxSize(size),
	m_size(0),
	m_lastIdGiven(-1),
	m_simulationDelay(0),
	m_simulationTime(0),
	m_gravity(-10),
	m_push(50.0f * SIMULATION_FREQUENCY),
	m_damping(0.002f),
	m_simulating(false),
	m_collisionDetectionMethod(MethodTree),
	m_useSimdOperations(false),
	m_diameter(1.0f),
	m_diameterSquared(m_diameter* m_diameter),
	m_radious(m_diameter / 2.0f)
{
	for (size_t i = 0; i < m_maxSize; i++)
	{
		m_positions[i] = vec2(0.0f, 0.0f);
		m_previousPositions[i] = vec2(0.0f, 0.0f);
		m_velocities[i] = vec2(0.0f);
		m_forces[i] = vec2(0.0f);

		m_mass[i] = 1000.0f;
		m_invMass[i] = 0.001f;
	}
}

Simulation2D::~Simulation2D()
{
	delete[] m_positions;
	delete[] m_previousPositions;
	delete[] m_velocities;
	delete[] m_forces;
	delete[] m_mass;
	delete[] m_invMass;
}


#pragma region MyRegion

void Simulation2D::update(double delta)
{
	{
		std::lock_guard<std::mutex> lock(g_simulationTimeMutex); // Lock the mutex
		m_simulationDelay += delta;
	}

	if (m_simulating)
		return;

	process();
}

#if MEASURE_PERFORMANCE
double g_time = 0;
uint32_t g_simulationSteps = 0;
uint64_t g_collisionCount = 0;
#endif

void Simulation2D::process()
{
	int steps = 0;

	while (m_simulationDelay >= FIXED_DELTA_TIME && steps < MAX_STEPS)
	{
#if MEASURE_PERFORMANCE
		auto start = std::chrono::high_resolution_clock::now();
#endif
		checkCollisions();

		{
			std::lock_guard<std::mutex> lock(g_externalForcesMutex);
			applyForces();
			step((float)FIXED_DELTA_TIME);
		}
		++steps;

		{
			std::lock_guard<std::mutex> lock(g_simulationTimeMutex); // Lock the mutex
			m_simulationDelay -= FIXED_DELTA_TIME;
			m_simulationTime += FIXED_DELTA_TIME;
		}

#if MEASURE_PERFORMANCE
		// Get the ending time
		auto end = std::chrono::high_resolution_clock::now();

		// Calculate the duration
		std::chrono::duration<double> duration = end - start;

		g_simulationSteps++;
		g_time += 1000 * duration.count();

		if (g_simulationSteps == 20 * SIMULATION_FREQUENCY)
		{
			auto average = g_time / g_simulationSteps;
			std::cout << average << "ms" << std::endl;
			std::cout << g_collisionCount << " collisions" << std::endl;
		}
#endif
	}
}

double Simulation2D::getSimulationTime()
{
	return m_simulationTime;
}

void Simulation2D::startSimulationThread()
{
	m_simulating = true;

	m_simulationThread = std::thread(&Simulation2D::runSimulationThread, this);
}

void Simulation2D::stopSimulationThread()
{
	if (!m_simulating)
		return;

	m_simulating = false;
	m_simulationThread.join();
}

// Track previous positions for lazy updates
std::vector<AABB> previousAABBs;
std::vector<vec2> previousAABBPositions;


void Simulation2D::checkCollisions()
{
	// Detect collisions
#if MEASURE_PERFORMANCE
	int checks = 0;
#endif
	m_collisions.clear();

	switch (m_collisionDetectionMethod)
	{
	case Simulation2D::None:
		break;
	case Simulation2D::MethodNaive:
	{
		for (size_t i = 0; i < m_size; i++)
		{
			// Simple collisions
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared < m_diameterSquared)
				{
					m_collisions.emplace_back(Collision(i, j, ij));
				}

#if MEASURE_PERFORMANCE
				checks++;
#endif
			}
		}
		break;
	}
	case  Simulation2D::MethodTree:
	{

		//std::lock_guard<std::mutex> lock(g_collisionTreeUpdateMutex);

		for (size_t i = m_tree.count; i < m_size; i++)
		{
			AABB boundinBox(0, 0, 0, 0);
			auto id = m_tree.insertObject(boundinBox);
			m_treeIDs.push_back(id);
			m_treeIdToSimulationID[id] = i;
			previousAABBs.push_back(boundinBox);
			previousAABBPositions.push_back(vec2(0.0f));
		}

		float scale = 1.5f;
		const float speedThreshold = 0.5f;
		// Update the objects' positions (example movement)
		for (size_t i = 0; i < m_treeIDs.size(); ++i)
		{
			auto& p = m_positions[i];

			float halfw = m_radious, halfh = m_radious;

			AABB updatedBox(
				p.x - halfw,
				p.y - halfw,
				p.x + halfh,
				p.y + halfh
			);

			// Update the object in the tree
			m_tree.updateObject(m_treeIDs[i], updatedBox);

			// Optimization ideas
			// Lazy update: only update the tree if the object has moved significantly
			//if (!previousAABBs[i].overlaps(updatedBox))
			//{
			//	m_tree.updateObject(m_treeIDs[i], updatedBox);
			//	previousAABBs[i] = updatedBox;
			//}
			//else {
			//	//std::cout << "Nope: " << i << std::endl;
			//}

			/*loat maxMovement = (scale - 1) * halfw;
			if (fabs(p.x - previousAABBPositions[i].x) > maxMovement || fabs(p.y - previousAABBPositions[i].y) > maxMovement)
			{
				AABB updatedBox(
					p.x - (scale * halfw),
					p.y - (scale * halfw),
					p.x + (scale * halfh),
					p.y + (scale * halfh)
				);

				m_tree.updateObject(m_treeIDs[i], updatedBox);
				previousAABBPositions[i] = p;
			}*/
		}


		std::vector<int> possibleCollisions;
		// Perform collision queries
		for (size_t i = 0; i < m_treeIDs.size(); ++i)
		{
			possibleCollisions.clear();
			m_tree.query(m_tree.nodes[m_treeIDs[i]].box, possibleCollisions);

			// Check actual collisions
			for (int id : possibleCollisions)
			{
				if (id != m_treeIDs[i] && m_tree.nodes[id].box.overlaps(m_tree.nodes[m_treeIDs[i]].box))
				{
					//std::cout << "Object " << m_treeIDs[i] << " is colliding with object " << id << std::endl;

					int a = i;
					int b = m_treeIdToSimulationID[id];

					if (a >= b)
						continue;

					vec2 ab = m_positions[b] - m_positions[a];

					float distanceSquared = (ab.x * ab.x) + (ab.y * ab.y);

					if (distanceSquared < m_diameterSquared)
					{
						vec2 vRel = m_velocities[b] - m_velocities[a];
						m_collisions.emplace_back(Collision(a, b, ab));
					}
#if MEASURE_PERFORMANCE
					checks++;
#endif
				}
			}
		}


		break;
	}
	default:
		break;
	}

#if MEASURE_PERFORMANCE
	if (g_simulationSteps == 0)
		std::cout << "First frame checks: " << checks << std::endl;
#endif
}

void Simulation2D::solveCollisionsPositionBased()
{
	// Calculate forces
	for (auto it = m_collisions.begin(); it != m_collisions.end(); ++it) {
		Collision col = *it;
		vec2 penetration = 0.5f * ((m_radious + m_radious) - length(col.AB)) * normalize(col.AB);

		m_positions[col.A] -= penetration;
		m_positions[col.B] += penetration;
	}
}



float EPSILON = 0.01f;

void Simulation2D::applyForces()
{
	// External forces
	if (m_externalForcesSinceLastUpdate && m_size - 1 == m_lastIdGiven)
	{
		m_externalForcesSinceLastUpdate = false;
		for (size_t i = 0; i < m_size; i++)
		{
			vec2& f = m_externalForces[i];
			m_forces[i] = f;
			m_externalForces[i] = vec2(0);
		}
	}

	// Attraction force
	bool attracttionEnabled = Input::GetKey(Input::G);
	if (attracttionEnabled) {
		for (size_t i = 0; i < m_size; i++)
		{
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared > m_diameterSquared) {
					vec2 attractionForce = (0.1f * (m_mass[i] * m_mass[j]) / distanceSquared) * normalize(ij);
					m_forces[i] += attractionForce;
					m_forces[j] -= attractionForce;
				}
			}
		}
	}

	bool repulsionEnabled = Input::GetKey(Input::H);
	if (repulsionEnabled) {
		for (size_t i = 0; i < m_size; i++)
		{
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared > m_diameterSquared) {
					vec2 attractionForce = -(0.1f * (m_mass[i] * m_mass[j]) / distanceSquared) * normalize(ij);
					m_forces[i] += attractionForce;
					m_forces[j] -= attractionForce;
				}
			}
		}
	}

	bool liftEnabled = Input::GetKey(Input::F);
	if (liftEnabled) {
		for (size_t i = 0; i < m_size / 2; i++)
		{
			m_forces[i] += vec2(0, 100000.0f);
		}
	}



	// Sphere collisions
	for (auto it = m_collisions.begin(); it != m_collisions.end(); ++it)
	{
		Collision col = *it;

		float lengthSquared = length2(col.AB);
		vec2 normal = lengthSquared > 0.0f ? normalize(col.AB) : vec2(1.0f);
		float penetration = (m_radious + m_radious) - length(col.AB);

		//// Position		
		//vec2 translation = 0.5f * penetration * normal;
		//m_positions[col.A] -= translation;
		//m_positions[col.B] += translation;



		// Impulse method
		float restitution = 0.5f;
		vec2 vRel = m_velocities[col.B] - m_velocities[col.A];
		float velocityAlongNormal = glm::dot(normal, vRel);
		float impulseMagnitude = -(1 + restitution) * velocityAlongNormal / (m_invMass[col.A] + m_invMass[col.B]);
		vec2 impulse = impulseMagnitude * normal;

		m_velocities[col.A] -= m_invMass[col.A] * impulse;
		m_velocities[col.B] += m_invMass[col.B] * impulse;


		// Penalty method
		vec2 penalty = m_push * penetration * normal;
		m_forces[col.A] -= m_mass[col.A] * penalty;
		m_forces[col.B] += m_mass[col.B] * penalty;


#if MEASURE_PERFORMANCE
		g_collisionCount++;
#endif
	}



	// Apply extra forces
	for (size_t i = 0; i < m_size; i++)
	{
		vec2& p = m_positions[i];
		//vec2& force = m_forces[i];

		// Gravity
		m_forces[i].y += m_mass[i] * m_gravity;

		// Bounds collisions
		// Wavy floor
		float a = 1.f;
		float d = p.y - a * sinf(0.5f * p.x + m_simulationTime);
		if (d < m_radious)
		{
			float penetration = (m_radious - d);

			// Collision normal calculation
			float d1 = p.y - a * sinf(0.5f * (p.x - EPSILON) + m_simulationTime);
			float d2 = (p.y - EPSILON) - a * sinf(0.5f * p.x + m_simulationTime);

			vec2 normal = vec2(d - d1, d - d2);
			normal = normalize(normal);

			// Position
			p += penetration * normal;


			// Impulse
			float restitution = 0.5f;
			vec2 vRel = -m_velocities[i];
			float velocityAlongNormal = glm::dot(normal, vRel);
			float impulseMagnitude = -(1 + restitution) * velocityAlongNormal; // * m_mass[i]; -> cancels out later 
			vec2 impulse = impulseMagnitude * normal;

			m_velocities[i] -= impulse; // * m_invMass[i]


			// Penalty
			vec2 v = penetration * normal;
			vec2 force = m_mass[i] * m_push * v;

			//force -= (10000.0f * m_damping * m_velocities[i]); // Drag ???

			m_forces[i] += force;

		}

		// Old walls
		if (Input::GetKey(Input::R))
		{
			if (p.x < m_radious) {
				p.x += m_radious - p.x;
				m_velocities[i].x = -0.5f * m_velocities[i].x;
			}
			else if (p.x > 30.0f - m_radious) {
				p.x -= m_radious - (30.0f - p.x);
				m_velocities[i].x = -0.5f * m_velocities[i].x;
			}
		}
	}


	for (auto it = m_springs.begin(); it != m_springs.end(); ++it)
	{
		Spring spring = *it;

		vec2 v = m_positions[spring.B] - m_positions[spring.A];
		vec2 n = normalize(v);
		float d = 1.0f - length(v);

		vec2 f = -0.5f * spring.K * d * (float)FIXED_DELTA_TIME * n;
		m_forces[spring.A] += f;
		m_forces[spring.B] -= f;
	}


}


void Simulation2D::step(float timeStep)
{

	// Integrate with verlet
	float invTimeStep = 1.0f / timeStep;
	for (size_t i = 0; i < m_size; i++)
	{
		// Acceleration
		vec2 acc = m_forces[i] * m_invMass[i] * timeStep * timeStep;

		// Store the current position
		vec2 currentPosition = m_positions[i];

		// Update the position using Verlet integration
		vec2 newPosition = m_positions[i] + (m_velocities[i]) * timeStep * (1.0f - m_damping) + (acc);
		m_positions[i] = newPosition;

		// Update the previous position
		//m_previousPositions[i] = currentPosition;

		// Reset forces
		m_forces[i] = vec2(0.0f);

		vec2 newVelocity = (newPosition - currentPosition) * invTimeStep;
		m_velocities[i] = newVelocity;
	}

	//Integrate with euler
	//for (size_t i = 0; i < m_size; i++)
	//{
	//	vec2 acc = m_forces[i] * m_invMass[i];
	//	m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
	//	m_positions[i] += timeStep * m_velocities[i];

	//	// Reset forces
	//	m_forces[i] = vec2(0.0f);

	//}

	/*m_positions[30] = vec2(0.0f, 15.0f);
	m_velocities[30] = vec2(0.0f);
	m_positions[59] = vec2(30.0f, 15.0f);
	m_velocities[59] = vec2(0.0f);*/

	for (auto it = m_fixedObjects.begin(); it != m_fixedObjects.end(); ++it)
	{
		SimulationID id = *it;

		m_positions[id] -= timeStep * m_velocities[id];
		m_velocities[id] = vec2(0.0f);
	}
}

#pragma endregion


SimulationID Simulation2D::generateSimulationID()
{
	//std::cout << (m_lastIdGiven + 1) << std::endl;
	return ++m_lastIdGiven;
}

size_t Simulation2D::getSize()
{
	return m_size;
}


void Simulation2D::addForce(SimulationID id, vec2 force)
{
	std::lock_guard<std::mutex> lock(g_externalForcesMutex);

	m_externalForcesSinceLastUpdate = true;
	m_externalForces[id] += SIMULATION_FREQUENCY * m_mass[id] * force;
}

void Simulation2D::addSpring(SimulationID a, SimulationID b, float stiffness)
{
	if (a == b)
		return;

	m_springs.emplace_back(a, b, stiffness);
}

void Simulation2D::fix(SimulationID id)
{
	m_fixedObjects.emplace_back(id);
}

void Simulation2D::unFix(SimulationID id)
{

}


vec2 Simulation2D::getPosition(SimulationID entity)
{
	return  m_positions[entity];
}

void Simulation2D::setPosition(SimulationID entity, vec2 pos)
{

	m_positions[entity] = pos;
	m_previousPositions[entity] = pos;
	//m_velocities[entity] = vec2(0.0f);
	//m_forces[entity] = vec2(0.0f);
	m_size = std::max((uint32_t)m_size, entity + 1);
}

void Simulation2D::updateTransform(Transform& transform, SimulationID entity)
{
	transform.position.x = m_positions[entity].x;
	transform.position.y = m_positions[entity].y;
}

SimulationID Simulation2D::raycast(vec2 pos)
{
	for (size_t i = 0; i < m_size; i++)
	{

		vec2 ij = pos - m_positions[i];

		float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

		if (distanceSquared < m_radious * m_radious)
		{
			return i;
		}


	}

	return -1;


	//std::lock_guard<std::mutex> lock(g_collisionTreeUpdateMutex);
	//AABB boundinBox(pos.x - 1.0, pos.y - 1.0, pos.x + 1.0, pos.y + 1.0);
	//std::vector<int> possibleCollisions;
	////possibleCollisions.reserve(3);
	//m_tree.query(boundinBox, possibleCollisions);

	//if (possibleCollisions.size() == 0)
	//	return 0;

	//return m_treeIDs[possibleCollisions[0]];
}




void Simulation2D::runSimulationThread()
{
	while (m_simulating)
	{
		process();
	}
}

