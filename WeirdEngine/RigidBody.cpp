#include "RigidBody.h"
#include "ECS.h"
#include "PhysicsEngine.h"
#include <iostream>

RigidBody::RigidBody(Entity* owner) :Component(owner)
{
	_collider = owner->GetComponent<Collider>();
	PhysicsEngine::GetInstance().Add(this);

	_inverseMass = 1.0 / _mass;

	_orientation = owner->Transform_->Rotation;

	float values[3][3]{ {0,0,0}, {0,0,0}, {0,0,0} }; // Collider.GetInertiaTensor() ????
	_inertiaTensor = _orientation.ToRotationMatrix() * Matrix3D(values);
}

void RigidBody::SetCollider(Collider* collider)
{
	_collider = collider;
}

void RigidBody::Fix()
{
	_inverseMass = 0;
	_applyGravity = false;
	_velocity = Vector3D();
	_angularVelocity = Vector3D();
}

void RigidBody::AddForce(Vector3D force)
{
	_force = _force + force;
}

void RigidBody::AddForce(Vector3D force, Vector3D position)
{
	_force = _force + force;
	_torque = _torque + 10000.0f * position.CrossProduct(force);
}

void RigidBody::ClearForces()
{
	_force.x = 0;
	_force.y = 0;
	_force.z = 0;

	_torque.x = 0;
	_torque.y = 0;
	_torque.z = 0;
}

Transform& RigidBody::GetTransform()
{
	return *_entity->Transform_;
}

Matrix3D RigidBody::GetUpdatedInvertedInertiaTensor()
{
	float i = 1;
	float j = 1;
	float k = 1;

	float values[3][3] = { {i,0,0}, {0,j,0}, {0,0,k} };
	auto newInertiaTensor = Matrix3D(values);

	auto orientationMatrix = _orientation.ToRotationMatrix();

	_inertiaTensor = orientationMatrix * newInertiaTensor;
	return _inertiaTensor;
}

Vector3D RigidBody::GetPosition()
{
	return _entity->Transform_->postition;
}
