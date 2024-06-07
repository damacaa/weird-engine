#include "Simulation.h"
#include <vector>
#include "../weird-engine/Input.h"


Simulation::Simulation(size_t size) :
	m_positions(new vec3[size]{ vec3(0.0f) }),
	m_velocities(new vec3[size]),
	m_forces(new vec3[size]),
	m_maxSize(size),
	m_size(0)
{

	for (size_t i = 0; i < m_maxSize; i++)
	{
		m_positions[i] = vec3(0.0f, 0.01f * i, 0.0f);
		m_velocities[i] = vec3(0.0f);
		m_forces[i] = vec3(0.0f);
	}

	//m_invMass = 1.0f / m_mass;
}

Simulation::~Simulation()
{
	delete[] m_positions;
	delete[] m_velocities;
	delete[] m_forces;
}

void Simulation::step(float delta)
{
	// TODO:     
	//std::for_each(std::execution::par, m_positions.begin(), m_positions.end(), [&](glm::vec3& p) { size_t i = &p - &m_positions[0]; } for parallel execution of the loop

	bool attracttionEnabled = Input::GetKey(Input::G);

	std::vector<Collision> collisions;
	// Detect collisions
	for (size_t i = 0; i < m_size; i++)
	{
		// Simple collisions
		for (size_t j = i + 1; j < m_size; j++)
		{
			vec3 ij = m_positions[j] - m_positions[i];

			float distance = length(ij);

			if (attracttionEnabled && distance > m_diameter) {
				// Attraction
				distance = distance < m_diameter ? m_diameter : distance;
				vec3 attractionForce = (1.0f * (m_mass * m_mass) / (distance * distance)) * normalize(ij);
				m_forces[i] += attractionForce;
				m_forces[j] -= attractionForce;
			}

			if (distance < 1.0f * m_diameter) {
				collisions.push_back(Collision(i, j, ij));
			}
		}
	}

	// Handle collisions
	for (size_t i = 0; i < collisions.size(); i++)
	{
		Collision col = collisions[i];
		vec3 force = m_mass * m_push * col.AB;

		m_forces[col.A] -= force;
		m_forces[col.B] += force;

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
				m_mass * m_push * (m_radious - p.y),
				0.0f
			);
		}
		else {
			// Gravity
			if (!attracttionEnabled)
				force.y += m_mass * m_gravity;
		}

		m_positions[i] = p;
		m_forces[i] = force;
	}

	// Integrate
	for (size_t i = 0; i < m_size; i++)
	{
		vec3 acc = m_forces[i] * m_invMass;
		m_velocities[i] += delta * (acc - (m_damping * m_velocities[i]));
		m_positions[i] += delta * m_velocities[i];

		// Reset forces
		m_forces[i] = vec3(0.0f);
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
		m_forces[i] = -(m_mass * f) * p + vec3(0.f, m_mass * f, 0.f);
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

