#include "RigidBody.h"
#include "ECS.h"
#include "PhysicsEngine.h"
#include <iostream>

RigidBody::RigidBody(Entity* owner) :Component(owner)
{
	m_collider = owner->GetComponent<Collider>();
	PhysicsEngine::GetInstance().Add(this);

	m_inverseMass = 1.0 / m_mass;

	orientation = owner->GetTransform().rotation;

	float values[3][3]{ {0,0,0}, {0,0,0}, {0,0,0} }; // Collider.GetInertiaTensor() ????
	m_inertiaTensor = orientation.ToRotationMatrix() * Matrix3D(values);
}

void RigidBody::SetCollider(Collider* collider)
{
	m_collider = collider;
}

void RigidBody::Fix()
{
	m_inverseMass = 0;
	applyGravity = false;
	velocity = Vector3D();
	angularVelocity = Vector3D();
}

void RigidBody::AddForce(Vector3D force)
{
	this->force = this->force + force;
}

void RigidBody::AddForce(Vector3D force, Vector3D position)
{
	this->force = this->force + force;
	torque = torque + 10000.0f * position.CrossProduct(force);
}

void RigidBody::ClearForces()
{
	force.x = 0;
	force.y = 0;
	force.z = 0;

	torque.x = 0;
	torque.y = 0;
	torque.z = 0;
}

Matrix3D RigidBody::GetUpdatedInvertedInertiaTensor()
{
	float i = 1;
	float j = 1;
	float k = 1;

	float values[3][3] = { {i,0,0}, {0,j,0}, {0,0,k} };
	auto newInertiaTensor = Matrix3D(values);

	auto orientationMatrix = orientation.ToRotationMatrix();

	m_inertiaTensor = orientationMatrix * newInertiaTensor;
	return m_inertiaTensor;
}

Vector3D RigidBody::GetPosition()
{
	return m_entity->GetTransform().postition;
}
