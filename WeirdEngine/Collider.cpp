#include "Collider.h"
#include "ECS.h"
#include "PhysicsEngine.h"

Collider::Collider(Entity* owner) :Component(owner) {
	_rb = _entity->GetComponent<RigidBody>();
	if (_rb != nullptr)
		_rb->SetCollider(this);

	PhysicsEngine::GetInstance().Add(this);

}
