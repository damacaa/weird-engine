#pragma once
#include "Component.h"
#include "Math.h"

class Collider;
class RigidBody : public Component
{
private:
	Collider* _collider;

	float _mass = 1;
	float _inverseMass = 0;
	Matrix3D _inertiaTensor;

public:

	bool _applyGravity = true;
	Vector3D _gravity = Vector3D(0, -9.8, 0);

	Vector3D _velocity = Vector3D(0, 0, 0);
	Vector3D _force = Vector3D(0, 0, 0);

	Quaternion _orientation = Quaternion();
	Vector3D _angularVelocity = Vector3D(0, 0, 0);
	Vector3D _torque = Vector3D(0, 0, 0);

	RigidBody(Entity* owner);

	void SetCollider(Collider* collider);

	Transform& GetTransform();

	void SetMass(float mass) {
		_mass = mass;
		_inverseMass = 1 / mass;
	}

	void Fix();

	float GetMass() { return _mass; }
	float GetInverseMass() { return _inverseMass; }

	void AddForce(Vector3D force);

	void AddForce(Vector3D force, Vector3D position);

	void ClearForces();

	Matrix3D GetUpdatedInvertedInertiaTensor();

	Vector3D GetPosition();

	Vector3D GetForce() { return _force; };
};



