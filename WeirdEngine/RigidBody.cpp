#include "RigidBody.h"
#include "ECS.h"
#include "PhysicsEngine.h"
#include <iostream>

RigidBody::RigidBody(Entity* owner) :Component(owner)
{
	m_collider = owner->GetComponent<Collider>();
	PhysicsEngine::GetInstance().Add(this);

	m_inverseMass = 1.0 / m_mass;

	position = owner->GetTransform().position;
	orientation = owner->GetTransform().rotation;

	float values[3][3]{ {0,0,0}, {0,0,0}, {0,0,0} };
	m_inertiaTensor = Matrix3D(values);
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
	torque = torque + position.CrossProduct(force);
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

	float w = m_entity->GetTransform().scale.x;
	float h = m_entity->GetTransform().scale.y;
	float d = m_entity->GetTransform().scale.z;

	float i = 0.083333f * m_mass * (w * w + d * d);
	float j = 0.083333f * m_mass * (d * d + h * h);
	float k = 0.083333f * m_mass * (w * w + h * h);

	float values[3][3] = { {1.0f / i, 0, 0}, {0, 1.0f / j, 0}, {0, 0, 1.0f / k} };
	auto newInertiaTensor = Matrix3D(values);

	auto orientationMatrix = orientation.ToRotationMatrix();

	m_inertiaTensor = orientationMatrix * newInertiaTensor;
	return m_inertiaTensor;
}

Vector3D RigidBody::GetPosition()
{
	return m_entity->GetTransform().position;
}

void RigidBody::Rotate(Vector3D axis, float amount)
{
	orientation = orientation * Quaternion(axis, amount);
}

void RigidBody::Update()
{
	m_entity->GetTransform().position = position;
	//m_entity->GetTransform().rotation = orientation;
}
