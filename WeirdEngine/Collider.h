#pragma once
#include "Component.h"

class RigidBody;
class Collider: public Component
{
protected:
	RigidBody* _rb;
public:
	Collider(Entity* owner);;
};

