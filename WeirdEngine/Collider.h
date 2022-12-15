#pragma once
#include "Component.h"

class RigidBody;
class BoxCollider;
class SphereCollider;
class Collider: public Component
{
protected:
	RigidBody* _rb;
	Collider(Entity* owner);
public:
	bool CheckCollision(BoxCollider* box) { return false; };
	bool CheckCollision(SphereCollider* sphere) { return false; };
};

