#pragma once
#include <vector>
#include "ECS.h"
class PhysicsEngine
{
private:

	std::vector<RigidBody*> _rigidBodies;
	std::vector<Collider*> _colliders;

	float _delta;
	int _substeps;

	Vector3D gravity = Vector3D(0, -9.8f, 0);

	PhysicsEngine() :_delta(0.02f), _substeps(3) {};

public:

	static PhysicsEngine& GetInstance() {
		static PhysicsEngine* _instance = new PhysicsEngine();
		return *_instance;
	};

	PhysicsEngine(PhysicsEngine& other) = delete;

	void operator=(const PhysicsEngine&) = delete;

	int Add(RigidBody* newRigidBody);

	int Add(Collider* collider);

	void Update();

	void Step(float delta);


	bool CheckCollision(int i, int j, CollisionInfo& collisionInfo);

	bool CheckCollision(SphereCollider& sphere, SphereCollider& otherSphere, CollisionInfo& collisionInfo);
	bool CheckCollision(BoxCollider& box, BoxCollider& otherBox, CollisionInfo& collisionInfo);
	bool CheckCollision(BoxCollider& box, SphereCollider& sphere, CollisionInfo& collisionInfo);

	void ResolveCollision(int i, int j, CollisionInfo& collisionInfo);

	void TestFunc();
};

