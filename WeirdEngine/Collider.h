#pragma once
#include "Component.h"
#include "Math.h"

class RigidBody;
class Collider : public Component
{
public:
	enum class ColliderType { Sphere, AABB };

protected:
	RigidBody* m_rb;

	Vector3D m_position = Vector3D(1, 1, 1);
	Vector3D m_rotation = Vector3D(1, 1, 1);
	Vector3D m_scale = Vector3D(1, 1, 1);

public:
	ColliderType type;

	Collider() :Component(), m_rb(nullptr), type(ColliderType::AABB) {};
	void SetUp(Entity* owner) override;

	float GetRadius() { return m_scale.x * 0.5f; };
	Vector3D GetPosition() { return m_position; };
	Vector3D GetRotation() { return m_rotation; };
	Vector3D GetScale() { return m_scale; };

	RigidBody& GetRigidBody() { return *m_rb; }

	void EnterCollision(Collider* collider) {
		m_entity->OnCollisionEnter(*collider);
	}

	void ExitCollision(Collider* collider) {
		m_entity->OnCollisionExit(*collider);
	}

	void Update() override;
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

