#include "Entity.h"
#include "ECS.h"
Entity::Entity()
{
	_components = std::vector<Component*>();
	Transform_ = new Transform();
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
