#include <iostream>

#include "PhysicsEngine.h"

void PhysicsEngine::Update()
{
	for (int i = 0; i < m_substeps; i++)
	{
		Step(m_delta / (float)m_substeps);
	}
}

void PhysicsEngine::Step(float delta)
{

	for (size_t i = 0; i < m_colliders.size(); i++)
	{
		for (size_t j = i + 1; j < m_colliders.size(); j++)
		{
			CollisionInfo collisionInfo;
			if (CheckCollision(i, j, collisionInfo)) {

				ResolveCollision(i, j, collisionInfo, delta);

				collisionInfo.a->EnterCollision(collisionInfo.b);
				collisionInfo.b->EnterCollision(collisionInfo.a);

				//m_collisions.push_back(collisionInfo);
			}
		}
	}

	for (size_t i = 0; i < m_rigidBodies.size(); i++)
	{
		auto rb = m_rigidBodies[i];

		Transform& t = rb->GetEntity().GetTransform();

		auto inverseMass = rb->GetInverseMass();

		Vector3D force = rb->GetForce() - (rb->linearDrag * delta * rb->GetMass() * rb->velocity); // Damping

		Vector3D acceleration = force * inverseMass;

		if (rb->applyGravity && inverseMass > 0)
		{
			acceleration = acceleration + gravity; // don ’t move infinitely heavy things
		}

		rb->velocity = rb->velocity + delta * acceleration;
		rb->position = rb->position + delta * rb->velocity;


		//Vector3D torque = rb->torque - (rb->angularDrag * delta * rb->angularVelocity); // Damping
		//Vector3D angAccel = rb->GetUpdatedInvertedInertiaTensor() * torque;
		//rb->angularVelocity = rb->angularVelocity + angAccel * delta;

		//Quaternion angularVelocityQuaternion = Quaternion(
		//	delta * 0.5f * rb->angularVelocity.x,
		//	delta * 0.5f * rb->angularVelocity.y,
		//	delta * 0.5f * rb->angularVelocity.z);

		//rb->orientation = rb->orientation * angularVelocityQuaternion;

		rb->ClearForces();
	}
}



bool PhysicsEngine::CheckCollision(int i, int j, CollisionInfo& collisionInfo)
{
	auto* a = m_colliders[i];
	auto* b = m_colliders[j];

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

	Vector3D delta = rigidBodyB.position - rigidBodyA.position;
	float distance = delta.Norm();

	float radiusA = sphere.GetRadius();
	float radiusB = otherSphere.GetRadius();

	float minDistance = radiusA + radiusB;

	float penetration = minDistance - distance;
	if (penetration > 0) {
		delta.Normalize();

		Vector3D pointA = radiusA * delta;
		Vector3D pointB = -(radiusB * delta);

		collisionInfo.AddContactPoint(pointA, pointB, delta, penetration);

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

	Vector3D delta = rigidBodyB.position - rigidBodyA.position;
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

		Vector3D maxA = rigidBodyA.position + (0.5f * transformA.scale);
		Vector3D minA = rigidBodyA.position - (0.5f * transformA.scale);

		Vector3D maxB = rigidBodyB.position + (0.5f * transformB.scale);
		Vector3D minB = rigidBodyB.position - (0.5f * transformB.scale);

		float distances[6]{
				(maxB.x - minA.x),
				(maxA.x - minB.x),
				(maxB.y - minA.y),
				(maxA.y - minB.y),
				(maxB.z - minA.z),
				(maxA.z - minB.z)
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

	Vector3D delta = rigidBodySphere.position - rigidBodyBox.position;

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



void PhysicsEngine::ResolveCollision(int i, int j, CollisionInfo& collisionInfo, float delta)
{
	RigidBody& rbA = collisionInfo.a->GetRigidBody();
	RigidBody& rbB = collisionInfo.b->GetRigidBody();

	// Pojection method
	float totalMass = rbA.GetInverseMass() + rbB.GetInverseMass();

	rbA.position = rbA.position - 100.0f * delta * (collisionInfo.point.penetration * (rbA.GetInverseMass() / totalMass) * collisionInfo.point.normal);
	rbB.position = rbB.position + 100.0f * delta * (collisionInfo.point.penetration * (rbB.GetInverseMass() / totalMass) * collisionInfo.point.normal);

	// Penalty method

	Vector3D force = 10000.0f * collisionInfo.point.normal * collisionInfo.point.penetration;

	rbA.AddForce(-rbA.restitution * (rbA.GetInverseMass() / totalMass) * force, collisionInfo.point.localA);
	rbB.AddForce(rbB.restitution * (rbB.GetInverseMass() / totalMass) * force, collisionInfo.point.localB);

	rbA.AddForce(-rbA.velocity, collisionInfo.point.localA);
	rbB.AddForce(-rbB.velocity, collisionInfo.point.localB);

	return;
	// Impulse method

	Vector3D relativeA = collisionInfo.point.localA - rbA.position;
	Vector3D relativeB = collisionInfo.point.localB - rbB.position;

	Vector3D angVelocityA = rbA.angularVelocity.CrossProduct(relativeA);
	Vector3D angVelocityB = rbB.angularVelocity.CrossProduct(relativeB);

	Vector3D fullVelocityA = rbA.velocity + angVelocityA;
	Vector3D fullVelocityB = rbB.velocity + angVelocityB;

	Vector3D contactVelocity = fullVelocityB - fullVelocityA;

	float impulseForce = contactVelocity.DotProduct(collisionInfo.point.normal);

	Vector3D inertiaA = (rbA.GetUpdatedInvertedInertiaTensor() * relativeA.CrossProduct(collisionInfo.point.normal)).CrossProduct(relativeA);
	Vector3D inertiaB = (rbB.GetUpdatedInvertedInertiaTensor() * relativeB.CrossProduct(collisionInfo.point.normal)).CrossProduct(relativeB);

	float angularEffect = (inertiaA + inertiaB).DotProduct(collisionInfo.point.normal);

	float cRestitution = 0.5f * (rbA.restitution + rbB.restitution);

	float impulseJ = (-(1.0f + cRestitution) * impulseForce) / (totalMass + angularEffect);

	Vector3D fullImpulse = impulseJ * collisionInfo.point.normal;

	rbA.velocity = rbA.velocity - (rbA.GetInverseMass() * fullImpulse);
	rbB.velocity = rbB.velocity + (rbB.GetInverseMass() * fullImpulse);
}

void PhysicsEngine::TestFunc()
{
	m_rigidBodies[2]->AddForce(Vector3D(0, 0, -1000), Vector3D(-1.0f, 0, 0));
}

int PhysicsEngine::Add(RigidBody* newRigidBody)
{
	m_rigidBodies.push_back(newRigidBody);

	return m_rigidBodies.size() - 1;
}

int PhysicsEngine::Add(Collider* collider)
{
	m_colliders.push_back(collider);

	return m_colliders.size() - 1;
}
