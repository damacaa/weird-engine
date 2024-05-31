#pragma once
#include "../weird-renderer/Shape.h" // TODO: replace with components
#include "../weird-engine/ecs/Entity.h"
#include "../weird-engine/ecs/Components/Transform.h"


using glm::vec3;
class Simulation
{
public:
	Simulation(size_t size);
	~Simulation();

	void Step(float delta);

	void Copy(Shape* target, size_t size);
	//void Copy(std::shared_ptr<ComponentArray<RigidBody>> target);


	void Shake(float f);
	void Push(vec3 v);



	vec3 GetPosition(Entity entity);
	void SetPosition(Entity entity, vec3 pos);

	void UpdateTransform(Transform& transform, Entity entity);

private:
	vec3* m_positions;
	vec3* m_velocities;
	vec3* m_forces;

	size_t m_size;

	const float m_mass = 100.0f;
	const float m_invMass = 1.0f / m_mass;

	const float m_diameter = 1.0f;
	const float m_radious = 0.5f * m_diameter;

	const float m_push = 1000.0f;
	const float m_damping = 1.0f;

	const float m_gravity = -9.81f;

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

