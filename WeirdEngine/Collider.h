#pragma once
#include "Component.h"
#include "Math.h"

class RigidBody;
class Collider : public Component
{
protected:
	RigidBody* m_rb;
	Collider(Entity* owner);
public:
	RigidBody& GetRigidBody() { return *m_rb; }

	enum class Type { Sphere, AABB };
	Type type;

	void EnterCollision(Collider* collider) {
		auto& components = m_entity->GetComponents();
		for (size_t i = 0; i < components.size(); i++)
		{
			components[i]->OnCollisionEnter(collider);
		}
	}

	void ExitCollision(Collider* collider) {
		auto& components = m_entity->GetComponents();
		for (size_t i = 0; i < components.size(); i++)
		{
			components[i]->OnCollisionExit(collider);
		}
	}
};

struct ContactPoint {
	Vector3D localA; // where did the collision occur ...
	Vector3D localB; // in the frame of each object !
	Vector3D normal;
	float penetration;
};

struct CollisionInfo {
	Collider* a;
	Collider* b;

	ContactPoint point;

	void AddContactPoint(const Vector3D& localA, const Vector3D& localB, const Vector3D& normal, float p) {
		point.localA = localA;
		point.localB = localB;
		point.normal = normal;
		point.penetration = p;
	}
};

