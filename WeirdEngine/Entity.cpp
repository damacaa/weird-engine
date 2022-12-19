#include "Entity.h"
#include "ECS.h"
Entity::Entity(std::string name)
{
	name = name;
	_components = std::vector<Component*>();
	m_transform = new Transform();
}

void Entity::Update()
{
	for (auto c : _components) {
		c->Update();
	}
}

void Entity::FixedUpdate()
{
	for (auto c : _components) {
		c->FixedUpdate();
	}
}

void Entity::Render()
{
	for (auto c : _components) {
		c->Render();
	}
}
