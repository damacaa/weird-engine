#pragma once
#include <vector>
#include "ECS.h"
class PhysicsEngine
{
private:

	std::vector<RigidBody*> m_rigidBodies;
	std::vector<Collider*> m_colliders;

	std::vector<CollisionInfo> m_collisions;

	float m_delta;
	int m_substeps;

	Vector3D gravity = Vector3D(0, -9.8f, 0);

	PhysicsEngine() :m_delta(0.01f), m_substeps(5) {};

public:

	static PhysicsEngine& GetInstance() {
		static PhysicsEngine* m_instance = new PhysicsEngine();
		return *m_instance;
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

	void ResolveCollision(int i, int j, CollisionInfo& collisionInfo, float delta);

	void TestFunc();
};

