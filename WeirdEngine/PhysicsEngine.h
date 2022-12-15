#pragma once
#include <vector>
#include "RigidBody.h"
#include "Transform.h"
class PhysicsEngine
{
private:

	static PhysicsEngine* _instance;

	std::vector<RigidBody*> rigidBodies;
	float _delta;

	PhysicsEngine() :_delta(0.02f) {};

public:

	static PhysicsEngine* GetInstance();

	PhysicsEngine(PhysicsEngine& other) = delete;

	void operator=(const PhysicsEngine&) = delete;

	void Update();

	int Add(RigidBody* newRigidBody);

};

