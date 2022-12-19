#pragma once
#include "Component.h"
#include "Math.h"

class Collider;
class RigidBody : public Component
{
private:
	Collider* m_collider;

	float m_mass = 1;
	float m_inverseMass = 0;
	Matrix3D m_inertiaTensor;

public:

	bool applyGravity = true;
	Vector3D gravity = Vector3D(0, -9.8, 0);

	Vector3D force = Vector3D(0, 0, 0);
	Vector3D velocity = Vector3D(0, 0, 0);
	Vector3D position = Vector3D(0, 0, 0);

	Vector3D torque = Vector3D(0, 0, 0);
	Vector3D angularVelocity = Vector3D(0, 0, 0);
	Quaternion orientation = Quaternion();

	RigidBody(Entity* owner);

	void SetCollider(Collider* collider);

	void SetMass(float mass) {
		m_mass = mass;
		m_inverseMass = 1 / mass;
	}

	float GetMass() { return m_mass; }
	float GetInverseMass() { return m_inverseMass; }

	void Fix();

	void AddForce(Vector3D force);

	void AddForce(Vector3D force, Vector3D position);

	void ClearForces();

	Matrix3D GetUpdatedInvertedInertiaTensor();

	Vector3D GetPosition();

	Vector3D GetForce() { return force; };

	void Update();
};



