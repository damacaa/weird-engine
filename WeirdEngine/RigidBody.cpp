#include "RigidBody.h"
#include "ECS.h"
#include "PhysicsEngine.h"
#include <iostream>

RigidBody::RigidBody(Entity* owner) :Component(owner)
{
	_collider = owner->GetComponent<Collider>();
	PhysicsEngine::GetInstance()->Add(this);

	_inverseMass = 1.0 / _mass;

	_orientation = Quaternion();

	float i[3][3]{ {0,0,0}, {0,0,0}, {0,0,0} }; // Collider.GetInertiaTensor() ????
	_inertiaTensor = Matrix3D(i);
}

void RigidBody::UpdatePhysics(float delta)
{

	if (_entity->Transform_->postition.y < -2)
	{
		_force = _force + Vector3D(0, -(_entity->Transform_->postition.y + 2), 0) * 1000;
	}

	Vector3D acceleration = _force * _inverseMass;

	if (_applyGravity && _inverseMass > 0)
	{
		acceleration = acceleration + _gravity; // don ’t move infinitely heavy things
	}

	_velocity = 0.95f * _velocity + delta * acceleration;
	_entity->Transform_->postition = _entity->Transform_->postition + delta * _velocity;


	// Rotation

	UpdateInertiaTensor();

	Vector3D angAccel = _inertiaTensor * _torque;
	_angularVelocity = 0.95f * _angularVelocity + angAccel * delta;

	//Quaternion a = Quaternion(delta * 0.5f * _angularVelocity, 1);
	//Quaternion b = a * _orientation;
	Quaternion c = Quaternion(delta * 0.5f * _angularVelocity.x, delta * 0.5f * _angularVelocity.y, delta * 0.5f * _angularVelocity.z);

	_orientation = _orientation * c;
	//_orientation.Normalize();

	_time = _time + 1;
	if (_time > 360)
		_time -= 360;

	_entity->Transform_->Rotation = _orientation;

	ClearForces();
}

void RigidBody::SetCollider(Collider* collider)
{
	_collider = collider;
}

void RigidBody::AddForce(Vector3D force)
{
	_force = _force + force;
}

void RigidBody::AddForce(Vector3D force, Vector3D position)
{
	_force = _force + force;
	Vector3D distance = position - _entity->Transform_->postition;
	_torque = _torque + 10.0f * distance.CrossProduct(force); // WHY *10????????
}

void RigidBody::CheckCollision(Collider* other)
{
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

Transform* RigidBody::GetTransform()
{
	return _entity->Transform_;
}

void RigidBody::UpdateInertiaTensor()
{
	float i = .2f * _mass * _entity->Transform_->scale.x;
	float _values[3][3] = { {i,0,0}, {0,i,0}, {0,0,i} };
	_inertiaTensor = Matrix3D(_values);
}

Vector3D RigidBody::GetPosition()
{
	return _entity->Transform_->postition;
}
