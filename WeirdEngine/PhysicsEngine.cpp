#include <iostream>

#include "PhysicsEngine.h"
#include "Collider.h"
#include <cassert>


void PhysicsEngine::Update()
{
	for (int i = 0; i < _substeps; i++)
	{
		Step(_delta / (float)_substeps);
	}
}

void PhysicsEngine::Step(float delta)
{

	for (size_t i = 0; i < _colliders.size(); i++)
	{
		for (size_t j = i + 1; j < _colliders.size(); j++)
		{
			CollisionInfo collisionInfo;
			if (CheckCollision(i, j, collisionInfo)) {
				ResolveCollision(i, j, collisionInfo);
			}
		}
	}

	for (size_t i = 0; i < _rigidBodies.size(); i++)
	{
		auto rb = _rigidBodies[i];

		Transform& t = rb->GetEntity().GetTransform();

		auto inverseMass = rb->GetInverseMass();

		Vector3D force = rb->GetForce() - (500.0f * delta * rb->velocity); // Damping

		Vector3D acceleration = force * inverseMass;

		if (rb->applyGravity && inverseMass > 0)
		{
			acceleration = acceleration + gravity; // don ’t move infinitely heavy things
		}

		rb->velocity = rb->velocity + delta * acceleration;
		rb->position = rb->position + delta * rb->velocity;


		Vector3D torque = rb->torque - (100.0f * delta rb->angularVelocity); // Damping
		Vector3D angAccel = rb->GetUpdatedInvertedInertiaTensor() * torque;
		rb->angularVelocity =  rb->angularVelocity + angAccel * delta;

		Quaternion angularVelocityQuaternion = Quaternion(
			delta * 0.5f * rb->angularVelocity.x,
			delta * 0.5f * rb->angularVelocity.y,
			delta * 0.5f * rb->angularVelocity.z);

		rb->orientation = rb->orientation * angularVelocityQuaternion;

		rb->ClearForces();
	}
}



bool PhysicsEngine::CheckCollision(int i, int j, CollisionInfo& collisionInfo)
{
	auto* a = _colliders[i];
	auto* b = _colliders[j];

	collisionInfo.a = a;
	collisionInfo.b = b;

	if (a->type == b->type) {

		if (a->type == Collider::Type::Sphere) {
			return CheckCollision((SphereCollider&)*a, (SphereCollider&)*b, collisionInfo);
		}
		else {
			return CheckCollision((BoxCollider&)*a, (BoxCollider&)*b, collisionInfo);
		}
	}
	else {

		if (a->type == Collider::Type::Sphere)
		{
			collisionInfo.a = b;
			collisionInfo.b = a;
			return CheckCollision((BoxCollider&)*b, (SphereCollider&)*a, collisionInfo);
		}
		else {
			return CheckCollision((BoxCollider&)*a, (SphereCollider&)*b, collisionInfo);
		}
	}
}

bool PhysicsEngine::CheckCollision(SphereCollider& sphere, SphereCollider& otherSphere, CollisionInfo& collisionInfo)
{
	auto& rigidBodyA = sphere.GetRigidBody();
	auto& rigidBodyB = otherSphere.GetRigidBody();

	Transform& transformA = rigidBodyA.GetEntity().GetTransform();
	Transform& transformB = rigidBodyB.GetEntity().GetTransform();

	Vector3D delta = transformB.position - transformA.position;
	float distance = delta.Norm();

	float radiusA = sphere.GetRadius();
	float radiusB = otherSphere.GetRadius();

	float minDistance = radiusA + radiusB;

	float penetration = minDistance - distance;
	if (penetration > 0) {
		delta.Normalize();

		Vector3D pointA = radiusA * delta;
		Vector3D pointB = -(radiusB * delta);

		collisionInfo.AddContactPoint(pointA, pointB, delta, penetration); // points where suposed to be in local

		return true;
	}

	return false;
}

bool PhysicsEngine::CheckCollision(BoxCollider& box, BoxCollider& otherBox, CollisionInfo& collisionInfo)
{
	auto& rigidBodyA = box.GetRigidBody();
	auto& rigidBodyB = otherBox.GetRigidBody();

	Transform& transformA = rigidBodyA.GetEntity().GetTransform();
	Transform& transformB = rigidBodyB.GetEntity().GetTransform();

	Vector3D delta = transformB.position - transformA.position;
	Vector3D totalSize = (transformA.scale * 0.5f) + (transformB.scale * 0.5f);

	if (abs(delta.x) < totalSize.x &&
		abs(delta.y) < totalSize.y &&
		abs(delta.z) < totalSize.z) {

		static const Vector3D faces[6] =
		{
			Vector3D(-1, 0, 0), Vector3D(1, 0, 0),
			Vector3D(0, -1, 0), Vector3D(0, 1, 0),
			Vector3D(0, 0, -1), Vector3D(0, 0, 1),
		};

		Vector3D maxA = transformA.position + transformA.scale;
		Vector3D minA = transformA.position - transformA.scale;

		Vector3D maxB = transformB.position + transformB.scale;
		Vector3D minB = transformB.position - transformB.scale;

		float distances[6]{
				(maxB.x - minA.x),// distance of box ’b’ to ’left ’ of ’a ’.
				(maxA.x - minB.x),// distance of box ’b’ to ’right ’ of ’a ’.
				(maxB.y - minA.y),// distance of box ’b’ to ’bottom ’ of ’a ’.
				(maxA.y - minB.y),// distance of box ’b’ to ’top ’ of ’a ’.
				(maxB.z - minA.z),// distance of box ’b’ to ’far ’ of ’a ’.
				(maxA.z - minB.z) // distance of box ’b’ to ’near ’ of ’a ’.
		};

		float penetration = FLT_MAX;
		Vector3D bestAxis;

		for (int i = 0; i < 6; i++)
		{
			if (distances[i] < penetration) {
				penetration = distances[i];
				bestAxis = faces[i];
			}
		}

		penetration *= 0.1f;

		collisionInfo.AddContactPoint(Vector3D(), Vector3D(), bestAxis, penetration);
		return true;
	}
	return false;

}

bool PhysicsEngine::CheckCollision(BoxCollider& box, SphereCollider& sphere, CollisionInfo& collisionInfo)
{
	auto& rigidBodyBox = box.GetRigidBody();
	auto& rigidBodySphere = sphere.GetRigidBody();

	Transform& transformBox = rigidBodyBox.GetEntity().GetTransform();
	Transform& transformSphere = rigidBodySphere.GetEntity().GetTransform();

	Vector3D halfSize = transformBox.scale * 0.5f;

	Vector3D delta = transformSphere.position - transformBox.position;

	// Matrix3D inverseRotationMatrix = transformBox.rotation.ToRotationMatrix().Inverse();
	// Vector3D inverseDelta = inverseRotationMatrix * delta;

	Vector3D closestPointOnBox = Vector3D::Clamp(delta, -halfSize, halfSize);

	Vector3D localPoint = delta - closestPointOnBox;
	float distance = localPoint.Norm();

	if (distance < sphere.GetRadius()) {// yes , we ’re colliding !
		Vector3D collisionNormal = localPoint.Normalized();
		float penetration = sphere.GetRadius() - distance;

		Vector3D boxPoint = Vector3D();
		Vector3D spherePoint = -(collisionNormal * sphere.GetRadius());

		collisionInfo.AddContactPoint(boxPoint, spherePoint, collisionNormal, penetration);
		return true;

	}
	return false;
}



void PhysicsEngine::ResolveCollision(int i, int j, CollisionInfo& collisionInfo)
{
	//collisionInfo.a->GetRigidBody()._velocity = -collisionInfo.a->GetRigidBody()._velocity;
	//collisionInfo.b->GetRigidBody()._velocity = -collisionInfo.b->GetRigidBody()._velocity;

	Vector3D force = 100.0f * collisionInfo.point.normal * collisionInfo.point.penetration;

	//collisionInfo.a->GetRigidBody().Fix();
	//collisionInfo.b->GetRigidBody().Fix();

	collisionInfo.a->GetRigidBody().AddForce(-force, collisionInfo.point.localA);
	collisionInfo.b->GetRigidBody().AddForce(force, collisionInfo.point.localB);
}

void PhysicsEngine::TestFunc()
{
	_rigidBodies[2]->AddForce(Vector3D(0, 0, -1000), Vector3D(1.5f, 1.5f, 1.5f));
}

int PhysicsEngine::Add(RigidBody* newRigidBody)
{
	_rigidBodies.push_back(newRigidBody);

	return _rigidBodies.size() - 1;
}

int PhysicsEngine::Add(Collider* collider)
{
	_colliders.push_back(collider);

	return _colliders.size() - 1;
}
