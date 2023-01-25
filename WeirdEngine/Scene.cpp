#include "Scene.h"
#include "PhysicsEngine.h"

#include <chrono>
#include <iostream>

Scene::Scene() : RED(1), GREEN(1), BLUE(1), ALPHA(1)
{
	glClearColor(RED, GREEN, BLUE, ALPHA);

	// Camera
	{
		Entity* camera = new Entity();
		camera->AddComponent<Camera>();
		camera->GetTransform().position = Vector3D(0, 0, 20);
		camera->GetTransform().scale = Vector3D(3, 3, 3);
		camera->GetTransform().rotation = Quaternion(0, 0, 0);
		camera->name = "Camera";
		camera->AddComponent<FlyMovement>();
		m_entities.push_back(camera);

		m_camera = camera;
	}

	// Floor
	if (true) {
		Entity* floor = new Entity();
		floor->name = "Floor";
		floor->GetTransform().position = Vector3D(0, -9, 0);
		floor->GetTransform().rotation = Quaternion();
		floor->GetTransform().scale = Vector3D(100, 10, 100);

		auto rb = floor->AddComponent<RigidBody>();
		rb->Fix();

		floor->AddComponent<BoxCollider>();

		auto renderer = floor->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		renderer->_color = Color(1, 1, 1);

		m_entities.push_back(floor);
	}
	else {
		Entity* floor = new Entity();
		floor->name = "Floor";
		floor->GetTransform().position = Vector3D(0, -102, 0);
		floor->GetTransform().rotation = Quaternion();
		floor->GetTransform().scale = Vector3D(200,200,200);

		auto rb = floor->AddComponent<RigidBody>();
		rb->Fix();

		floor->AddComponent<SphereCollider>();

		auto renderer = floor->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::BigSphere;
		renderer->_color = Color(1, 1, 1);

		m_entities.push_back(floor);
	}

	if (false)
	{
		Entity* entity = new Entity();

		auto rb = entity->AddComponent<RigidBody>();

		rb->applyGravity = false;
		rb->Fix();

		entity->AddComponent<SphereCollider>();
		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
		entity->GetTransform().rotation = Quaternion();
		entity->GetTransform().scale = Vector3D(2, 2, 2);
		entity->GetTransform().position = Vector3D(0, 0, 0);
		entity->name = "Test";
		m_entities.push_back(entity);
	}

	if (false)
	{
		Entity* entity = new Entity();

		entity->name = "Test";

		entity->GetTransform().rotation = Quaternion(0, 0, 45);
		entity->GetTransform().scale = Vector3D(3, 1, 3);
		entity->GetTransform().position = Vector3D(-1, 4, 0);

		auto rb = entity->AddComponent<RigidBody>();
		entity->AddComponent<BoxCollider>();

		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;

		m_entities.push_back(entity);
	}

	srand((unsigned)time(NULL));
	for (size_t i = 0; i < 10; i++)
	{
		Entity* entity = new Entity();
		entity->name = "Box";
		entity->GetTransform().position = Vector3D(
			10.0f * (((float)rand() / (RAND_MAX)) - 0.5f),
			(3 * i) + 6,
			10.0f * (((float)rand() / (RAND_MAX))) - 0.5f);

		entity->GetTransform().rotation = Quaternion();
		entity->GetTransform().scale = Vector3D(1, 1, 1) * (1 + 3 * ((double)rand() / (RAND_MAX)));

		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(1, 1, 1);
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;

		auto rb = entity->AddComponent<RigidBody>();
		entity->AddComponent<BoxCollider>();

		m_entities.push_back(entity);
	}
}

void Scene::Update()
{
	for (auto e : m_entities)
	{
		e->Update();
	}

	// Ball shooting
	if (Input::GetMouseButtonDown(Input::MouseButton::LeftClick)) {
		AddBall();
	}

	if (Input::GetMouseButtonDown(Input::MouseButton::RightClick)) {
		PhysicsEngine::GetInstance().TestFunc();
	}
}

void Scene::FixedUpdate()
{
	for (auto e : m_entities)
	{
		e->FixedUpdate();
	}

	PhysicsEngine::GetInstance().Update();
}

void Scene::Render()
{
	for (auto e : Renderer::Instances())
	{
		e->Render();
	}
}

void Scene::AddBall()
{
	Entity* entity = new Entity();
	entity->name = "Ball_";
	entity->GetTransform().position = m_camera->GetTransform().position + 5.0f * m_camera->GetTransform().GetForwardVector();

	entity->GetTransform().rotation = Quaternion();
	entity->GetTransform().scale = Vector3D(1,1,1);

	auto renderer = entity->AddComponent<PrimitiveRenderer>();
	renderer->_color = Color(1, 1, 1);

	auto rb = entity->AddComponent<RigidBody>();
	rb->SetMass(0.5f);

	renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
	entity->AddComponent<SphereCollider>();

	rb->velocity = 25.0f * m_camera->GetTransform().GetForwardVector();

	m_entities.push_back(entity);
}


