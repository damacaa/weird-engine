#pragma once
#include "Collider.h"
class SphereCollider : public Collider
{
public:
	SphereCollider(Entity* owner) :Collider(owner) {};
};

