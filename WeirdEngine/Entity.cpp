#include "Entity.h"
#include "ECS.h"
Entity::Entity(std::string name)
{
	name = name;
	m_components = std::vector<Component*>();
	m_transform = new Transform();
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
