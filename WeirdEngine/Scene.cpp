#include "Scene.h"
#include "PhysicsEngine.h"

void Scene::Update()
{
	for (auto e : _entities)
	{
		e->Update();
	}
}

void Scene::FixedUpdate()
{
	for (auto e : _entities)
	{
		e->FixedUpdate();
	}

	for (auto e : RigidBody::Instances())
	{
	}

	PhysicsEngine::GetInstance()->Update();
}

void Scene::Render()
{
	for (auto e : Renderer::Instances())
	{
		e->Render();
	}
}


