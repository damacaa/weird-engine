#include "FlyMovement.h"
#include "ECS.h"


FlyMovement::FlyMovement(Entity* owner) :Component(owner)
{
	//m_entity->AddComponent<SphereCollider>();

	Vector3D defaultRotation = m_entity->GetTransform().rotation.ToEuler();
	m_angleX = defaultRotation.x;
	m_angleY = defaultRotation.y;
}

void FlyMovement::Update()
{

	// Camera movement
	auto rb = m_entity->GetComponent<RigidBody>();
	Vector3D velocity = Vector3D();

	if (Input::GetInstance().GetKey('w')) {
		velocity = velocity + m_entity->GetTransform().GetForwardVector();
	}
	else if (Input::GetInstance().GetKey('s')) {
		velocity = velocity - m_entity->GetTransform().GetForwardVector();
	}

	if (Input::GetInstance().GetKey('a')) {
		velocity = velocity - m_entity->GetTransform().GetRightVector();
	}
	else if (Input::GetInstance().GetKey('d')) {
		velocity = velocity + m_entity->GetTransform().GetRightVector();
	}

	if (Input::GetInstance().GetKey(' ')) {
		velocity = velocity + Vector3D(0, 1, 0);
	}
	else if (Input::GetInstance().GetKey('x')) {
		velocity = velocity + Vector3D(0, -1, 0);
	}

	m_entity->GetTransform().position = m_entity->GetTransform().position + 30.0f * Time::deltaTime * velocity;

	m_angleX += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaY();
	m_angleX = std::max(-75.0f, std::min(m_angleX, 75.0f));

	m_angleY += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaX();

	m_entity->GetTransform().rotation = Quaternion(m_angleX, m_angleY, 0);
}
