#include "FlyMovement.h"
#include "ECS.h"


FlyMovement::FlyMovement(Entity* owner) :Component(owner)
{
	auto rb = m_entity->AddComponent<RigidBody>();
	rb->applyGravity = false;
	m_entity->AddComponent<SphereCollider>();

	Vector3D defaultRotation = m_entity->GetTransform().rotation.ToEuler();
	m_angleX = defaultRotation.x;
	m_angleY = defaultRotation.y;
}

void FlyMovement::Update()
{
	// Camera movement
	auto rb = m_entity->GetComponent<RigidBody>();

	if (Input::GetInstance().GetKey('w')) {
		rb->AddForce(100.0f * m_entity->GetTransform().GetForwardVector());
	}
	else if (Input::GetInstance().GetKey('s')) {
		rb->AddForce(-100.0f * m_entity->GetTransform().GetForwardVector());
	}

	if (Input::GetInstance().GetKey('a')) {
		rb->AddForce(-100.0f * m_entity->GetTransform().GetRightVector());
	}
	else if (Input::GetInstance().GetKey('d')) {
		rb->AddForce(100.0f * m_entity->GetTransform().GetRightVector());
	}

	if (Input::GetInstance().GetKey(' ')) {
		rb->AddForce(Vector3D(0, 100, 0));
	}
	else if (Input::GetInstance().GetKey('x')) {
		rb->AddForce(Vector3D(0, -100, 0));
	}

	m_angleX += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaY();
	m_angleX = std::max(-75.0f, std::min(m_angleX, 75.0f));

	m_angleY += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaX();
	
	rb->orientation = Quaternion(m_angleX, m_angleY, 0);
}
