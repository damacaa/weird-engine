#include "RigidBody.h"
#include "ECS.h"

RigidBody::RigidBody(Entity* owner) :Component(owner)
{
	_colliders = owner->GetComponents<Collider>();
}

void RigidBody::Update()
{
	///_entity->Transform_->postition.x += 0.01f;
	//_entity->Transform_->postition.y += 0.01f;
	//_entity->Transform_->postition.z += 0.01f;

	_entity->Transform_->eulerRotation.x += 90.0f * Time::DeltaTime;
	_entity->Transform_->eulerRotation.y += 90.0f * Time::DeltaTime;
	//_entity->Transform_->eulerRotation.z += 90.0f * delta;

	//_entity->Transform_->postition.y += 0.5  *delta;
}

void RigidBody::FixedUpdate()
{
	_f = _f + Vector3D(0, -9.8 * _mass, 0);

	if (_entity->Transform_->postition.y < -2) {
		_f = _f + Vector3D(0, -(_entity->Transform_->postition.y + 2), 0) * 10000;
	}

	Vector3D a = _f / _mass;
	_velocity = _velocity + Time::FixedDeltaTime * a;
	_entity->Transform_->postition = _entity->Transform_->postition + Time::FixedDeltaTime * _velocity;

	_f.x = 0;
	_f.y = 0;
	_f.z = 0;
}

void RigidBody::AddCollider(Collider* collider)
{
	_colliders.push_back(collider);
}

void RigidBody::AddForce(Vector3D& force)
{
	_f = _f + force;
}
