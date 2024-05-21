#pragma once
#include "../weird-renderer/Shape.h"
using glm::vec3;
class Simulation
{
public:
	Simulation(Shape* data, size_t size);
	Simulation() {};
	~Simulation();

	void Step(float delta);
	void Copy(Shape* target);

	void Shake(float f);
	void Push(vec3 v);

	void SetPositions(Shape* data);
private:
	vec3* m_positions;
	vec3* m_velocities;
	vec3* m_forces;

	size_t m_size;

	float m_mass = 10000.0f;
	float m_invMass = 1.0f;

	float m_diameter = 1.0f;
	float m_radious = 0.5f;

	float m_push = 1000.0f;
	float m_damping = 0.0f;

	float m_gravity = -9.81f;

	struct Collision
	{
	public:
		Collision(int a, int b, vec3 ab) {
			A = a;
			B = b;
			AB = ab;
		}

		int A;
		int B;
		vec3 AB;
	};
};

