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
		camera->GetTransform().scale = Vector3D(3,3,3);
		camera->GetTransform().rotation = Quaternion(0, 0, 0);
		camera->name = "Camera";
		auto rb = camera->AddComponent<RigidBody>();
		rb->applyGravity = false;
		camera->AddComponent<SphereCollider>();
		m_entities.push_back(camera);

		m_camera = camera;
	}

	// Floor
	{
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
		entity->name = "Ball_";
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
	if (Input::GetInstance().GetKeyDown('p')) {
		AddBall();
	}

	// Camera movement
	auto rb = m_camera->GetComponent<RigidBody>();

	if (Input::GetInstance().GetKey('w')) {
		rb->AddForce(Vector3D(0, 0, -100));
	}
	else if (Input::GetInstance().GetKey('s')) {
		rb->AddForce(Vector3D(0, 0, 100));
	}

	if (Input::GetInstance().GetKey('a')) {
		rb->AddForce(Vector3D(-100, 0, 0));
	}
	else if (Input::GetInstance().GetKey('d')) {
		rb->AddForce(Vector3D(100, 0, 0));
	}

	if (Input::GetInstance().GetMouseButtonDown(Input::MouseButton::LeftClick)) {
		AddBall();
	}

	//m_camera->GetTransform().Rotate(Vector3D(0, 1, 0), 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaX());
	rb->Rotate(Vector3D(0, 1, 0), 10000.0f * Time::deltaTime * Input::GetInstance().GetMouseDeltaX());

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
	entity->GetTransform().position = m_camera->GetTransform().position - Vector3D(0, 0, 3);

	entity->GetTransform().rotation = Quaternion();
	entity->GetTransform().scale = Vector3D(1, 1, 1) * (1 + 1 * ((double)rand() / (RAND_MAX)));

	auto renderer = entity->AddComponent<PrimitiveRenderer>();
	renderer->_color = Color(1, 1, 1);

	auto rb = entity->AddComponent<RigidBody>();

	renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
	entity->AddComponent<SphereCollider>();

	rb->AddForce(Vector3D(1000.0f * (((double)rand() / (RAND_MAX)) - .5f), 500, -5000));

	m_entities.push_back(entity);
}


