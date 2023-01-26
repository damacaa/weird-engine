#include "Collider.h"
#include "ECS.h"
#include "PhysicsEngine.h"


void Collider::SetUp(Entity* owner)
{
	Component::SetUp(owner);

	m_rb = m_entity->GetComponent<RigidBody>();

	if (m_rb != nullptr)
		m_rb->SetCollider(this);

	
	m_position = m_entity->GetTransform().position;
	m_rotation = m_entity->GetTransform().rotation.ToEuler();
	m_scale = m_entity->GetTransform().scale;
}

void Collider::Update()
{
}
