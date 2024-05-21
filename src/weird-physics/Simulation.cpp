#include "Simulation.h"
#include <vector>

Simulation::Simulation(Shape* data, size_t size)
{
	m_positions = new vec3[size];
	m_velocities = new vec3[size];
	m_forces = new vec3[size];
	m_size = size;

	for (size_t i = 0; i < m_size; i++)
	{
		m_positions[i] = data[i].position;
		m_velocities[i] = vec3(0.0f);
		m_forces[i] = vec3(0.0f);
	}

	m_invMass = 1.0f / m_mass;
}

Simulation::~Simulation()
{
	delete[] m_positions;
	delete[] m_velocities;
	delete[] m_forces;
}

void Simulation::Step(float delta)
{
	// TODO:     
	//std::for_each(std::execution::par, m_positions.begin(), m_positions.end(), [&](glm::vec3& p) { size_t i = &p - &m_positions[0]; } for parallel execution of the loop


	std::vector<Collision> collisions;
	// Detect collisions
	for (size_t i = 0; i < m_size; i++)
	{
		// Simple collisions
		for (size_t j = i + 1; j < m_size; j++)
		{
			vec3 ij = m_positions[j] - m_positions[i];

			// Attraction
			float d = length(ij);
			d = d < 1.0f ? 1.0f : d;
			vec3 g = 100.0f * (m_mass * normalize(ij)) / (d * d);

			//m_forces[i] += g;
			//m_forces[j] -= g;

			if (length(ij) < 1.0f * m_diameter) {
				vec3 half = 0.5f * ij;
				collisions.push_back(Collision(i, j, half));
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
	}

	// Calculate forces
	for (size_t i = 0; i < m_size; i++)
	{
		vec3 force = m_forces[i];
		vec3 p = m_positions[i];

		// Floor at y = 0
		if (p.y < m_radious) {

			p.y += (m_radious - p.y);

			force += vec3(
				0.0f,
				-m_mass * m_push * (m_radious - p.y),
				0.0f
			);
		}

		// Gravity
		force.y += m_mass * m_gravity;

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

void Simulation::Copy(Shape* target)
{
	for (size_t i = 0; i < m_size; i++)
	{
		target[i].position = m_positions[i];
	}
}

void Simulation::Shake(float f)
{
	for (size_t i = 0; i < m_size; i++)
	{
		vec3 p = 0.123456f * m_positions[i];
		m_forces[i] = -(m_mass * f) * p + vec3(0.f, m_mass * f, 0.f);
		//m_forces[i] += 50.0f * vec3(sin(p.x + i), cos(p.y + i), 1 * cos(3.14 * sin(p.z + i)));
	}
}

void Simulation::Push(vec3 v)
{
	m_forces[0] = v;
}

void Simulation::SetPositions(Shape* data)
{
	for (size_t i = 0; i < m_size; i++)
	{
		m_positions[i] = data[i].position;
		m_velocities[i] = vec3(0.0f);
		m_forces[i] = vec3(0.0f);
	}
}
