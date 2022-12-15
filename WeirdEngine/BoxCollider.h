#pragma once
#include "Collider.h"
class BoxCollider : public Collider
{
public:
	BoxCollider(Entity* owner) :Collider(owner) {};
};

