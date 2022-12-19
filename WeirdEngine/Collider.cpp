#include "Collider.h"
#include "ECS.h"
#include "PhysicsEngine.h"

Collider::Collider(Entity* owner) :Component(owner) {
	m_rb = m_entity->GetComponent<RigidBody>();
	if (m_rb != nullptr)
		m_rb->SetCollider(this);

	PhysicsEngine::GetInstance().Add(this);

}
