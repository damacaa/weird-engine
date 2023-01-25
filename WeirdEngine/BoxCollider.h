#pragma once
#include "Collider.h"
class BoxCollider : public Collider
{
public:
	BoxCollider() :Collider() { type = Type::AABB; };
};

