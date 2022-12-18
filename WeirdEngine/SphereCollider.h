#pragma once
#include "Collider.h"
#include "Transform.h"
class SphereCollider : public Collider
{
public:
	SphereCollider(Entity* owner) :Collider(owner) { type = Type::Sphere; };
	float GetRadius() { return _entity->Transform_->scale.x / 2; };
};

