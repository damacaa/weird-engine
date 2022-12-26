#include "FlyMovement.h"
#include "ECS.h"


FlyMovement::FlyMovement(Entity* owner) :Component(owner)
{
	auto rb = m_entity->AddComponent<RigidBody>();
	rb->applyGravity = true;
	m_entity->AddComponent<SphereCollider>();

	Vector3D defaultRotation = m_entity->GetTransform().rotation.ToEuler();
	m_angleX = defaultRotation.x;
	m_angleY = defaultRotation.y;
}

void FlyMovement::Update()
{

	// Camera movement
	auto rb = m_entity->GetComponent<RigidBody>();
	Vector3D direction = Vector3D();

	if (Input::GetInstance().GetKey('w')) {
		direction = direction + m_entity->GetTransform().GetForwardVector();
	}
	else if (Input::GetInstance().GetKey('s')) {
		direction = direction - m_entity->GetTransform().GetForwardVector();
	}

	if (Input::GetInstance().GetKey('a')) {
		direction = direction - m_entity->GetTransform().GetRightVector();
	}
	else if (Input::GetInstance().GetKey('d')) {
		direction = direction + m_entity->GetTransform().GetRightVector();
	}

	if (Input::GetInstance().GetKey(' ')) {
		direction = direction + Vector3D(0, 1, 0);
	}
	else if (Input::GetInstance().GetKey('x')) {
		direction = direction + Vector3D(0, -1, 0);
	}

	//rb->AddForce(100.0f * direction);
	rb->velocity = 20.0f * direction;

	m_angleX += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaY();
	m_angleX = std::max(-75.0f, std::min(m_angleX, 75.0f));

	m_angleY += 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaX();

	m_entity->GetTransform().rotation = Quaternion(m_angleX, m_angleY, 0);
}
