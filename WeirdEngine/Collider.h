#pragma once
#include "Component.h"
#include "Math.h"

class RigidBody;
class Collider : public Component
{
public:
	enum class Type { Sphere, AABB };

protected:
	RigidBody* m_rb;

public:
	Type type;

	Collider():Component(), m_rb(nullptr), type(Type::AABB) {};
	void SetUp(Entity* owner) override;

	RigidBody& GetRigidBody() { return *m_rb; }

	void EnterCollision(Collider* collider) {
		m_entity->OnCollisionEnter(*collider);
	}

	void ExitCollision(Collider* collider) {
		m_entity->OnCollisionExit(*collider);
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

