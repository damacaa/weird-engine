#include "PhysicsEngine.h"
#include <iostream>

PhysicsEngine* PhysicsEngine::_instance = nullptr;;

PhysicsEngine* PhysicsEngine::GetInstance()
{
	if (_instance == nullptr) {
		_instance = new PhysicsEngine();
	}
	return _instance;
}

void PhysicsEngine::Update()
{
	// Divide space
	// Collisions

	for (auto rb : rigidBodies)
	{
		Vector3D force = Vector3D(1, 0, 0);
		//rb->AddForce(rb->GetEntity()->Transform_->postition * -1);

		for (auto otherRb : rigidBodies)
		{
			if (rb == otherRb)
				continue;

			Transform* a = rb->GetTransform();
			Transform* b = otherRb->GetTransform();

			Vector3D v = b->postition - a->postition;
			float minDistance = a->scale.x / 2 + b->scale.x / 2;
			float distance = v.Norm();

			float penetration = minDistance - distance;
			if (penetration > 0) {
				v.Normalize();
				//rb->AddForce(v * penetration * -1.0f);
				//otherRb->AddForce(v * penetration);

				rb->AddForce(100.0f*v * penetration * -1.0f, a->postition + (a->scale.x * v));
				otherRb->AddForce(100.0f*v * penetration, b->postition - (b->scale.x * v));
			}

		}

		rb->UpdatePhysics(_delta);
	}
}

int PhysicsEngine::Add(RigidBody* newRigidBody)
{
	rigidBodies.push_back(newRigidBody);

	return rigidBodies.size() - 1;
}
