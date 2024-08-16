#include "Simulation.h"
#include "CollisionDetection/SpatialHash.h"
#include "../weird-engine/Input.h"
#include "CollisionDetection/Octree.h"

#include <chrono>
#include <immintrin.h>

using namespace std::chrono;

constexpr double FIXED_DELTA_TIME = 1 / 100.0;
constexpr size_t MAX_STEPS = 10;

Simulation::Simulation(size_t size) :
	m_positions(new vec3[size]{ vec3(0.0f) }),
	m_velocities(new vec3[size]),
	m_forces(new vec3[size]),
	m_mass(new float[size]),
	m_invMass(new float[size]),
	m_maxSize(size),
	m_size(0),
	m_simulationDelay(0),
	m_collisionDetectionMethod(MethodUniformGrid),
	m_simulating(false),
	m_useSimdOperations(false)
{

	for (size_t i = 0; i < m_maxSize; i++)
	{
		m_positions[i] = vec3(0.0f, 0.01f * i, 0.0f);
		m_velocities[i] = vec3(0.0f);
		m_forces[i] = vec3(0.0f);

		m_mass[i] = 100.0f;
		m_invMass[i] = 0.01f;
	}

}

Simulation::~Simulation()
{
	delete[] m_positions;
	delete[] m_velocities;
	delete[] m_forces;
	delete[] m_mass;
	delete[] m_invMass;
}

void Simulation::update(double delta)
{
	m_simulationDelay += delta;

	int steps = 0;
	while (m_simulationDelay >= FIXED_DELTA_TIME && steps < MAX_STEPS)
	{
		checkCollisions();
		step((float)FIXED_DELTA_TIME);
		m_simulationDelay -= FIXED_DELTA_TIME;
		++steps;
	}

	//if (steps >= MAX_STEPS)
	//	std::cout << "Not enough steps for simulation" << std::endl;

}

void Simulation::startSimulationThread()
{
	m_simulating = true;

	m_simulationThread = std::thread(&Simulation::runSimulationThread, this);
}

void Simulation::stopSimulationThread()
{
	m_simulating = false;
	m_simulationThread.join();
}

void Simulation::checkCollisions()
{
	// Detect collisions
	int checks = 0;

	//m_collisionCount = 0;
	m_collisions.clear();

	switch (m_collisionDetectionMethod)
	{
	case None:
		break;
	case NaiveMethod:
	{
		for (size_t i = 0; i < m_size; i++)
		{
			// Simple collisions
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec3 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y) + (ij.z * ij.z);

				if (distanceSquared < m_diameterSquared) {
					m_collisions.push_back(Collision(i, j, ij));
				}

				checks++;
			}
		}
	}
	break;
	case MethodUniformGrid:
	{
		SpatialHash spatialHash(2.0f);

		for (size_t i = 0; i < m_size; i++)
		{
			auto pos = m_positions[i];
			spatialHash.insert(pos, m_radious, i);
		}

		for (size_t i = 0; i < m_size; i++)
		{
			auto pos = m_positions[i];
			auto possibleCollisions = spatialHash.retrieve(pos, m_radious);

			//std::cout << "Possible collisions with query sphere: ";
			for (int id : possibleCollisions) {

				checks++;

				vec3 ij = m_positions[id] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y) + (ij.z * ij.z);

				if (distanceSquared < m_diameterSquared) {
					m_collisions.push_back(Collision(i, id, ij));
				}
			}
		}
	}
	break;
	case OctreeMethod:
	{
		// Create an Octree with a center at (0, 0, 0) and a half-size of 100 units
		Octree octree({ 0.0f, 0.0f, 0.0f }, 10.0f);

		for (size_t i = 0; i < m_size; i++)
		{
			auto pos = m_positions[i];
			octree.insert(pos, m_radious, i);
		}


		for (size_t i = 0; i < m_size; i++)
		{
			auto pos = m_positions[i];
			auto possibleCollisions = octree.retrieve(pos, m_radious);

			for (int id : possibleCollisions) {

				checks++;

				vec3 ij = m_positions[id] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y) + (ij.z * ij.z);

				if (distanceSquared < m_diameterSquared) {
					m_collisions.push_back(Collision(i, id, ij));
				}
			}
		}
	}
	break;
	default:
		break;
	}


	checks; // Add a breakpoint to check how many collision checks were calculated
}

void Simulation::step(float timeStep)
{
	// TODO:     
	//std::for_each(std::execution::par, m_positions.begin(), m_positions.end(), [&](glm::vec3& p) { size_t i = &p - &m_positions[0]; } for parallel execution of the loop

	// Attraction force
	bool attracttionEnabled = Input::GetKey(Input::G);
	if (attracttionEnabled) {
		for (size_t i = 0; i < m_size; i++)
		{
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec3 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y) + (ij.z * ij.z);

				if (distanceSquared > m_diameterSquared) {
					vec3 attractionForce = (1.0f * (m_mass[i] * m_mass[j]) / distanceSquared) * normalize(ij);
					m_forces[i] += attractionForce;
					m_forces[j] -= attractionForce;
				}
			}
		}
	}


	// Handle collisions
	for (size_t i = 0; i < m_collisions.size(); i++) //m_collisionCount
	{
		Collision col = m_collisions[i];
		vec3 force = m_mass[i] * m_push * col.AB;

		m_forces[col.A] -= force;
		m_forces[col.B] += force;

		// Position based solver
		// vec3 t = 0.01f * (length(col.AB) - m_diameter) * normalize(col.AB);
		// m_positions[col.A] += t;
		// m_positions[col.B] -= t;
	}

	// Calculate forces
	for (size_t i = 0; i < m_size; i++)
	{
		vec3 force = m_forces[i];
		vec3 p = m_positions[i];

		// Floor at y = 0
		if (p.y < m_radious) {

			p.y += 0.5f * (m_radious - p.y);
			//p.y = (m_radious);

			force += vec3(
				0.0f,
				m_mass[i] * m_push * (m_radious - p.y),
				0.0f
			);
		}
		else {
			// Gravity
			if (!attracttionEnabled)
				force.y += m_mass[i] * m_gravity;
		}

		m_positions[i] = p;
		m_forces[i] = force;
	}

	if (m_useSimdOperations)
	{
		const __m256 vec_timeStep = _mm256_set1_ps(timeStep);
		const __m256 vec_damping = _mm256_set1_ps(m_damping);
		const __m256 vec_zero = _mm256_setzero_ps();

		size_t i = 0;
		for (; i + 2 <= m_size; i += 2) {  // Processing two vec3 elements (6 floats) at a time

			// Load forces and inverse masses
			__m256 force1 = _mm256_loadu_ps(&m_forces[i].x);
			__m256 force2 = _mm256_loadu_ps(&m_forces[i + 1].x);
			__m256 invMass = _mm256_set_ps(m_invMass[i + 1], m_invMass[i + 1], m_invMass[i + 1], m_invMass[i + 1],
				m_invMass[i], m_invMass[i], m_invMass[i], m_invMass[i]);


			// Calculate accelerations
			__m256 acc1 = _mm256_mul_ps(force1, invMass);
			__m256 acc2 = _mm256_mul_ps(force2, invMass);

			// Load velocities
			__m256 vel1 = _mm256_loadu_ps(&m_velocities[i].x);
			__m256 vel2 = _mm256_loadu_ps(&m_velocities[i + 1].x);

			// Calculate new velocities
			__m256 dampingVel1 = _mm256_mul_ps(vec_damping, vel1);
			__m256 dampingVel2 = _mm256_mul_ps(vec_damping, vel2);

			__m256 newVel1 = _mm256_add_ps(vel1, _mm256_mul_ps(vec_timeStep, _mm256_sub_ps(acc1, dampingVel1)));
			__m256 newVel2 = _mm256_add_ps(vel2, _mm256_mul_ps(vec_timeStep, _mm256_sub_ps(acc2, dampingVel2)));

			// Store new velocities
			_mm256_storeu_ps(&m_velocities[i].x, newVel1);
			_mm256_storeu_ps(&m_velocities[i + 1].x, newVel2);

			// Calculate new positions
			__m256 newPos1 = _mm256_add_ps(_mm256_loadu_ps(&m_positions[i].x), _mm256_mul_ps(vec_timeStep, newVel1));
			__m256 newPos2 = _mm256_add_ps(_mm256_loadu_ps(&m_positions[i + 1].x), _mm256_mul_ps(vec_timeStep, newVel2));

			// Store new positions
			_mm256_storeu_ps(&m_positions[i].x, newPos1);
			_mm256_storeu_ps(&m_positions[i + 1].x, newPos2);

			// Reset forces
			_mm256_storeu_ps(&m_forces[i].x, vec_zero);
			_mm256_storeu_ps(&m_forces[i + 1].x, vec_zero);
		}

		// Handle remaining elements
		for (; i < m_size; ++i) {
			vec3 acc = m_forces[i] * m_invMass[i];
			m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
			m_positions[i] += timeStep * m_velocities[i];
			m_forces[i] = vec3{ 0.0f, 0.0f, 0.0f };
		}

	}
	else {
		// Integrate
		for (size_t i = 0; i < m_size; i++)
		{
			vec3 acc = m_forces[i] * m_invMass[i];
			m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
			m_positions[i] += timeStep * m_velocities[i];

			// Reset forces
			m_forces[i] = vec3(0.0f);
		}
	}
}

SimulationID Simulation::generateSimulationID()
{
	return m_size++;
}

size_t Simulation::getSize()
{
	return m_size;
}



void Simulation::shake(float f)
{
	for (size_t i = 0; i < m_size; i++)
	{
		vec3 p = 0.123456f * m_positions[i];
		m_forces[i] = -(m_mass[i] * f) * p + vec3(0.f, m_mass[i] * f, 0.f);
		//m_forces[i] += 50.0f * vec3(sin(p.x + i), cos(p.y + i), 1 * cos(3.14 * sin(p.z + i)));
	}
}

void Simulation::push(vec3 v)
{
	m_forces[0] = v;
}


vec3 Simulation::getPosition(SimulationID entity)
{
	return m_positions[entity];
}

void Simulation::setPosition(SimulationID entity, vec3 pos)
{
	m_positions[entity] = pos;
	m_velocities[entity] = vec3(0.0f);
	m_forces[entity] = vec3(0.0f);
}

void Simulation::updateTransform(Transform& transform, SimulationID entity)
{
	transform.position = m_positions[entity];
}




void Simulation::runSimulationThread()
{

	auto start = high_resolution_clock::now();
	auto end = high_resolution_clock::now();

	while (m_simulating)
	{
		end = high_resolution_clock::now();
		duration<double, std::milli> iteration_time = end - start;
		double duration = 0.001 * iteration_time.count();
		start = high_resolution_clock::now();
		if (duration < FIXED_DELTA_TIME) 
		{
			std::this_thread::sleep_for(milliseconds((int)(1000 * (FIXED_DELTA_TIME - duration))));
		}
		else 
		{
			update(duration);
		}
	}
}

