#pragma once
#include "Component.h"
#include "Math.h"

class RigidBody;
class BoxCollider;
class SphereCollider;
class Collider : public Component
{
protected:
	RigidBody* _rb;
	Collider(Entity* owner);
public:
	RigidBody& GetRigidBody() { return *_rb; }

	enum class Type { Sphere, AABB };
	Type type;
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

