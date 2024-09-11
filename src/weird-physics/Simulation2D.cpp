#include "Simulation2D.h"
#include "CollisionDetection/SpatialHash.h"
#include "../weird-engine/Input.h"
#include "CollisionDetection/Octree.h"

#include <chrono>
#include <immintrin.h>
#include <mutex>

#define MEASURE_PERFORMANCE true		


using namespace std::chrono;


constexpr float SIMULATION_FREQUENCY = 500.0;
constexpr double FIXED_DELTA_TIME = 1 / SIMULATION_FREQUENCY;

constexpr size_t MAX_STEPS = 100;

float g_worldSideLenght = 30.0f;

std::mutex g_simulationTimeMutex;



Simulation2D::Simulation2D(size_t size) :
	m_positions(new vec2[size]),
	m_velocities(new vec2[size]),
	m_forces(new vec2[size]),
	m_externalForcesSinceLastUpdate(false),
	m_externalForces(new vec2[size]),
	m_mass(new float[size]),
	m_invMass(new float[size]),
	//m_smallCollisionPairs(new bool[(size * (size + 1)) / 2]),
	m_smallCollisionPairs(new bool[size * size]),
	m_maxSize(size),
	m_size(0),
	m_simulationDelay(0),
	m_simulationTime(0),
	m_gravity(-10),
	m_push(20.0f * SIMULATION_FREQUENCY),
	m_damping(0.1),
	m_simulating(false),
	m_collisionDetectionMethod(MethodUniformGrid),
	grid(20.0f * g_worldSideLenght, 2.0f * m_diameter),
	m_useSimdOperations(false)
{



	for (size_t i = 0; i < m_maxSize; i++)
	{
		m_positions[i] = vec2(0.0f, 0.01f * i);
		m_velocities[i] = vec2(0.0f);
		m_forces[i] = vec2(0.0f);

		m_mass[i] = 1000.0f;
		m_invMass[i] = 0.001f;
	}

}

Simulation2D::~Simulation2D()
{
	delete[] m_positions;
	delete[] m_velocities;
	delete[] m_forces;
	delete[] m_mass;
	delete[] m_invMass;
	delete[] m_smallCollisionPairs;
}

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
		applyForces();
		step((float)FIXED_DELTA_TIME);
		++steps;

		std::lock_guard<std::mutex> lock(g_simulationTimeMutex); // Lock the mutex
		m_simulationDelay -= FIXED_DELTA_TIME;
		m_simulationTime += FIXED_DELTA_TIME;

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
			std::cout << g_collisionCount << " checks" << std::endl;
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
	m_simulating = false;
	m_simulationThread.join();
}


void Simulation2D::checkCollisions()
{
	// Detect collisions
	int checks = 0;
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

				checks++;
			}
		}
		break;
	}
	case Simulation2D::MethodUniformGrid:
	{
		// Clear grid
		grid.clear();

		// Add elements to grid
		for (size_t i = 0; i < m_size; i++)
		{
			grid.addElement(i, m_positions[i]);
		}

		int cellsPerSide = grid.getCellCountPerSide();

		int maxSize = (m_maxSize * (m_maxSize + 1)) / 2;

		for (int x = 1; x < cellsPerSide - 1; x++)
		{
			for (int y = 1; y < cellsPerSide - 1; y++)
			{
				std::vector<int> cells;

				for (int offsetX = -1; offsetX <= 1; offsetX++)
				{
					for (int offsetY = -1; offsetY <= 1; offsetY++)
					{
						auto& objectsInCell = grid.getCell(x + offsetX, y + offsetY);
						if (objectsInCell.size() > 0)
							cells.insert(cells.end(), objectsInCell.begin(), objectsInCell.end());
					}
				}


				for (size_t i = 0; i < cells.size(); i++)
				{
					// Simple collisions
					for (size_t j = i + 1; j < cells.size(); j++)
					{
						int a = cells[i];
						int b = cells[j];

						size_t pairId = (b * m_maxSize) + a;// -(b * (b + 1) / 2);
						if (m_smallCollisionPairs[pairId])
							continue;

						if (a == b) {
							continue;
						}

						vec2 ab = m_positions[b] - m_positions[a];

						float distanceSquared = (ab.x * ab.x) + (ab.y * ab.y);

						if (distanceSquared < m_diameterSquared)
						{
							m_smallCollisionPairs[pairId] = true;
							m_collisions.emplace_back(Collision(a, b, ab));
						}

						checks++;
					}
				}
			}
		}
	}
	default:
		break;
	}
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
	if (m_externalForcesSinceLastUpdate) {
		m_externalForcesSinceLastUpdate = false;
		for (size_t i = 0; i < m_size; i++)
		{
			m_forces[i] = m_externalForces[i];
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

		if (m_collisionDetectionMethod == MethodUniformGrid)
		{
			size_t pairId = (col.B * m_maxSize) + col.A;// -(col.B * (col.B + 1) / 2);
			m_smallCollisionPairs[pairId] = false;
		}

		vec2 normal = normalize(col.AB);
		float penetration = (m_radious + m_radious) - length(col.AB);

		// Position
		/*
		vec2 translation = 0.25f * penetration * normal;
		m_positions[col.A] -= translation;
		m_positions[col.B] += translation;
		*/


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
		float a = 2.5f;
		float d = p.y - a * sinf(0.5f * p.x);
		if (d < m_radious)
		{
			float penetration = (m_radious - d);

			// Collision normal calculation
			float d1 = p.y - a * sinf(0.5f * (p.x - EPSILON));
			float d2 = (p.y - EPSILON) - a * sinf(0.5f * p.x);

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

			force -= (10000.0f * m_damping * m_velocities[i]); // Drag ???

			m_forces[i] += force;

		}

		// Old walls
		if (p.x < m_radious) {
			p.x += m_radious - p.x;
			m_velocities[i].x = -0.5f * m_velocities[i].x;
		}
		else if (p.x > g_worldSideLenght - m_radious) {
			p.x -= m_radious - (g_worldSideLenght - p.x);
			m_velocities[i].x = -0.5f * m_velocities[i].x;
		}
	}
}

void Simulation2D::step(float timeStep)
{
	// Integrate
	for (size_t i = 0; i < m_size; i++)
	{
		vec2 acc = m_forces[i] * m_invMass[i];
		m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
		m_positions[i] += timeStep * m_velocities[i];

		// Reset forces
		m_forces[i] = vec2(0.0f);
	}


	// Step through cells
	/*std::vector<int> cells;
	for (size_t x = 0; x < 4; x++)
	{
		for (size_t y = 0; y < 2; y++)
		{
			auto& objectsInCell = grid.getCell(x, y);
			if (objectsInCell.size() > 0)
				cells.insert(cells.end(), objectsInCell.begin(), objectsInCell.end());
		}
	}


	// Integrate
	for (size_t c = 0; c < cells.size(); c++)
	{
		int i = cells[c];
		m_forces[i].y += m_mass[i] * m_gravity;

		vec2 acc = m_forces[i] * m_invMass[i];
		m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
		m_positions[i] += timeStep * m_velocities[i];

		// Reset forces
		m_forces[i] = vec2(0.0f);
	}*/
}

SimulationID Simulation2D::generateSimulationID()
{
	return m_size++;
}

size_t Simulation2D::getSize()
{
	return m_size;
}



void Simulation2D::shake(float f)
{
	for (size_t i = 0; i < m_size; i++)
	{
		vec2 p = 0.123456f * m_positions[i];
		m_forces[i] = -(m_mass[i] * f) * p + vec2(0.f, m_mass[i] * f);
		//m_forces[i] += 50.0f * vec3(sin(p.x + i), cos(p.y + i), 1 * cos(3.14 * sin(p.z + i)));
	}
}

void Simulation2D::push(vec2 v)
{
	m_forces[0] = v;
}

void Simulation2D::addForce(SimulationID id, vec2 force)
{
	m_externalForcesSinceLastUpdate = true;
	m_externalForces[id] += SIMULATION_FREQUENCY * m_mass[id] * force;
}


vec2 Simulation2D::getPosition(SimulationID entity)
{
	return m_positions[entity];
}

void Simulation2D::setPosition(SimulationID entity, vec2 pos)
{
	m_positions[entity] = pos;
	m_velocities[entity] = vec2(0.0f);
	m_forces[entity] = vec2(0.0f);
}

void Simulation2D::updateTransform(Transform& transform, SimulationID entity)
{
	transform.position.x = m_positions[entity].x;
	transform.position.y = m_positions[entity].y;
}




void Simulation2D::runSimulationThread()
{
	while (m_simulating)
	{
		process();
	}
}

