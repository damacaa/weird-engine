#include "Entity.h"
#include "ECS.h"
Entity::Entity(std::string name)
{
	this->name = name;
	m_components = std::vector<Component*>();
	m_transform = new Transform();
}

Entity::~Entity()
{
	for (auto it = begin(m_components); it != end(m_components); ++it) {
		delete* it;
	}

	delete m_transform;
}

void Entity::OnCollisionEnter(Collider& collider)
{
	for (auto it = begin(m_components); it != end(m_components); ++it) {
		(*it)->OnCollisionEnter(collider);
	}
}

void Entity::OnCollisionExit(Collider& collider)
{
	for (auto it = begin(m_components); it != end(m_components); ++it) {
		(*it)->OnCollisionExit(collider);
	}
}
