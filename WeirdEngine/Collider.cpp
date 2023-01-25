#include "Collider.h"
#include "ECS.h"
#include "PhysicsEngine.h"


void Collider::SetUp(Entity* owner)
{
	Component::SetUp(owner);

	m_rb = m_entity->GetComponent<RigidBody>();

	if (m_rb != nullptr)
		m_rb->SetCollider(this);
}
