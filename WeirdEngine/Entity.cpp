#include "Entity.h"
#include "ECS.h"
Entity::Entity(std::string name)
{
	name = name;
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

void Entity::Update()
{
	for (auto c : m_components) {
		c->Update();
	}
}

void Entity::FixedUpdate()
{
	for (auto c : m_components) {
		c->FixedUpdate();
	}
}

void Entity::Render()
{
	for (auto c : m_components) {
		c->Render();
	}
}
