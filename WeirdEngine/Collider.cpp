#include "Collider.h"
#include "ECS.h"

Collider::Collider(Entity* owner) :Component(owner) {
	_rb = _entity->GetComponent<RigidBody>();
	if (_rb != nullptr)
		_rb->SetCollider(this);
}
