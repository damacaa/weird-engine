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

	bool _applyGravity = true;
	Vector3D _gravity = Vector3D(0, -9.8, 0);

	Vector3D _velocity = Vector3D(0, 0, 0);
	Vector3D _force = Vector3D(0, 0, 0);
	
	Quaternion _orientation = Quaternion();
	Vector3D _angularVelocity = Vector3D(0, 0, 0);
	Vector3D _torque = Vector3D(0, 0, 0);
	Matrix3D _inertiaTensor;

	float _time;

public:
	RigidBody(Entity* owner);

	void UpdatePhysics(float delta);

	void SetCollider(Collider* collider);

	void AddForce(Vector3D force);

	void AddForce(Vector3D force, Vector3D position);

	void CheckCollision(Collider* other);

	void ClearForces();

	void UpdateInertiaTensor();

	Transform* GetTransform();

	Vector3D GetPosition();
};

