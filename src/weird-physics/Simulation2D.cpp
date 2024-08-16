#include "Simulation2D.h"
#include "CollisionDetection/SpatialHash.h"
#include "../weird-engine/Input.h"
#include "CollisionDetection/Octree.h"

#include <chrono>
#include <immintrin.h>
#include "CollisionDetection/UniformGrid2D.h"

using namespace std::chrono;

constexpr double FIXED_DELTA_TIME = 1 / 1000.0;
constexpr size_t MAX_STEPS = 2;

float side = 30.0f;


Simulation2D::Simulation2D(size_t size) :
	m_positions(new vec2[size]),
	m_velocities(new vec2[size]),
	m_forces(new vec2[size]),
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
		m_positions[i] = vec2(0.0f, 0.01f * i);
		m_velocities[i] = vec2(0.0f);
		m_forces[i] = vec2(0.0f);

		m_mass[i] = 100.0f;
		m_invMass[i] = 0.01f;
	}

}

Simulation2D::~Simulation2D()
{
	delete[] m_positions;
	delete[] m_velocities;
	delete[] m_forces;
	delete[] m_mass;
	delete[] m_invMass;
}

void Simulation2D::update(double delta)
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
	//	std::cout << "Not enough steps for Simulation2D" << std::endl;

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

UniformGrid2D grid(10.0f * side, 3.0f);

void Simulation2D::checkCollisions()
{
	// Detect collisions
	int checks = 0;
	m_collisions.clear();

	switch (m_collisionDetectionMethod)
	{
	case Simulation2D::None:
		break;
	case Simulation2D::NaiveMethod:
	{

		for (size_t i = 0; i < m_size; i++)
		{
			// Simple collisions
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared < m_diameterSquared) {
					m_collisions.insert(Collision(i, j, ij));
				}

				checks++;
			}
		}
		break;
	}
	case Simulation2D::MethodUniformGrid:
	{

		for (size_t substep = 0; substep < 8; substep++)
		{


			grid.clear();

			for (size_t i = 0; i < m_size; i++)
			{
				grid.addElement(i, m_positions[i]);
			}

			int cellsPerSide = grid.getCellCountPerSide();

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

							vec2 ab = m_positions[b] - m_positions[a];

							float distanceSquared = (ab.x * ab.x) + (ab.y * ab.y);

							if (distanceSquared < m_diameterSquared) {
								m_collisions.insert(Collision(a, b, ab));
							}

							checks++;
						}
					}



				}
			}

			break;
		}
	}
	default:
		break;
	}
}

void Simulation2D::step(float timeStep)
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
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared > m_diameterSquared) {
					vec2 attractionForce = (1.0f * (m_mass[i] * m_mass[j]) / distanceSquared) * normalize(ij);
					m_forces[i] += attractionForce;
					m_forces[j] -= attractionForce;
				}
			}
		}
	}


	// Calculate forces
	for (auto it = m_collisions.begin(); it != m_collisions.end(); ++it) {
		Collision col = *it;

		vec2 force = m_mass[col.A] * m_push * col.AB;

		m_forces[col.A] -= force;
		m_forces[col.B] += force;
	}

	for (size_t i = 0; i < m_size; i++)
	{
		//vec2& force = m_forces[i];
		vec2& p = m_positions[i];

		// Floor at y = 0
		if (p.y < m_radious) {

			p.y += (m_radious - p.y);
			m_velocities[i].y = -0.9f * m_velocities[i].y;

		}

		if (p.x < m_radious) {
			p.x += m_radious - p.x;
			m_velocities[i].x = -0.9f * m_velocities[i].x;
		}
		else if (p.x > side - m_radious) {
			p.x -= m_radious - (side - p.x);
			m_velocities[i].x = -0.9f * m_velocities[i].x;
		}
	}





	// Integrate
	for (size_t i = 0; i < m_size; i++)
	{
		m_forces[i].y += m_mass[i] * m_gravity;

		vec2 acc = m_forces[i] * m_invMass[i];
		m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
		m_positions[i] += timeStep * m_velocities[i];

		// Reset forces
		m_forces[i] = vec2(0.0f);
	}

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
	transform.position = glm::vec3(m_positions[entity], 0.0f);
}




void Simulation2D::runSimulationThread()
{

	auto start = high_resolution_clock::now();
	auto end = high_resolution_clock::now();


	while (m_simulating)
	{
		end = high_resolution_clock::now();
		duration<double, std::milli> iteration_time = end - start;
		double duration = 0.001 * iteration_time.count();
		start = high_resolution_clock::now();
		update(duration);


	}

}

