#include "Scene.h"

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
		Vector3D force = Vector3D(1, 0, 0);
		((RigidBody*)e)->AddForce(force);
	}
}

void Scene::Render()
{
	for (auto e : Renderer::Instances())
	{
		e->Render();
	}
}


