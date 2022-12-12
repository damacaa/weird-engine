#pragma once
#include "Component.h"
#include "Math.h"

class Collider;
class RigidBody : public Component
{
private:
	std::vector<Collider*> _colliders;

	float _mass = 1;
	Vector3D _velocity = Vector3D(0, 0, 0);
	Vector3D _f = Vector3D(0, 0, 0);

public:
	RigidBody(Entity* owner);

	void Update();
	void FixedUpdate();

	void AddCollider(Collider* collider);

	void AddForce(Vector3D& force);
};

