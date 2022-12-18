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
		camera->Transform_->postition = Vector3D(0, 0, 20);
		camera->Transform_->Rotation = Quaternion(0, 0, 0);
		camera->Name = "Camera";
		auto rb = camera->AddComponent<RigidBody>();
		rb->_applyGravity = false;
		camera->AddComponent<SphereCollider>();
		_entities.push_back(camera);

		_camera = camera;
	}

	// Floor
	{
		Entity* floor = new Entity();
		floor->Name = "Floor";
		floor->Transform_->postition = Vector3D(0, -9, 0);
		floor->Transform_->Rotation = Quaternion();
		floor->Transform_->scale = Vector3D(100, 10, 100);

		auto rb = floor->AddComponent<RigidBody>();
		rb->Fix();

		floor->AddComponent<BoxCollider>();

		auto renderer = floor->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		renderer->_color = Color(1, 1, 1);

		_entities.push_back(floor);
	}

	if (false)
	{
		Entity* entity = new Entity();

		auto rb = entity->AddComponent<RigidBody>();

		rb->_applyGravity = false;
		rb->Fix();

		entity->AddComponent<SphereCollider>();
		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
		entity->Transform_->Rotation = Quaternion();
		entity->Transform_->scale = Vector3D(2, 2, 2);
		entity->Transform_->postition = Vector3D(0, 0, 0);
		entity->Name = "Test";
		_entities.push_back(entity);
	}

	if (false)
	{
		Entity* entity = new Entity();

		entity->Name = "Test";

		entity->Transform_->Rotation = Quaternion(0, 0, 45);
		entity->Transform_->scale = Vector3D(3, 1, 3);
		entity->Transform_->postition = Vector3D(-1, 4, 0);

		auto rb = entity->AddComponent<RigidBody>();
		entity->AddComponent<BoxCollider>();

		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;

		_entities.push_back(entity);
	}

	srand((unsigned)time(NULL));
	for (size_t i = 0; i < 10; i++)
	{
		Entity* entity = new Entity();
		entity->Name = "Ball_";
		entity->Transform_->postition = Vector3D(
			10.0f * (((float)rand() / (RAND_MAX)) - 0.5f),
			(3 * i) + 6,
			10.0f * (((float)rand() / (RAND_MAX))) - 0.5f);

		entity->Transform_->Rotation = Quaternion();
		entity->Transform_->scale = Vector3D(1, 1, 1) * (1 + 3 * ((double)rand() / (RAND_MAX)));

		auto renderer = entity->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(1, 1, 1);

		auto rb = entity->AddComponent<RigidBody>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		entity->AddComponent<BoxCollider>();

		_entities.push_back(entity);
	}
}

void Scene::Update()
{
	for (auto e : _entities)
	{
		e->Update();
	}

	// Ball shooting
	if (Input::GetKeyDown('p')) {
		AddBall();
	}

	// Camera movement
	auto rb = _camera->GetComponent<RigidBody>();

	if (Input::GetKey('w')) {
		rb->AddForce(Vector3D(0, 0, -100));
	}
	else if (Input::GetKey('s')) {
		rb->AddForce(Vector3D(0, 0, 100));
	}

	if (Input::GetKey('a')) {
		rb->AddForce(Vector3D(-100, 0, 0));
	}
	else if (Input::GetKey('d')) {
		rb->AddForce(Vector3D(100, 0, 0));
	}

	if (Input::GetMouseButtonDown(Input::MouseButton::LeftClick)) {
		AddBall();
	}

	//_camera->Transform_->Rotate(Vector3D(0, 1, 0), 10000.0f * Time::DeltaTime * Input::GetMouseDeltaX());
	//_camera->Transform_->Rotate(Vector3D(1, 0, 0), 10000.0f * Time::DeltaTime * Input::GetMouseDeltaY());
}

void Scene::FixedUpdate()
{
	for (auto e : _entities)
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
	entity->Name = "Ball_";
	entity->Transform_->postition = _camera->Transform_->postition - Vector3D(0, 0, 3);

	entity->Transform_->Rotation = Quaternion();
	entity->Transform_->scale = Vector3D(1, 1, 1) * (1 + 1 * ((double)rand() / (RAND_MAX)));

	auto renderer = entity->AddComponent<PrimitiveRenderer>();
	renderer->_color = Color(1, 1, 1);

	auto rb = entity->AddComponent<RigidBody>();

	renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
	entity->AddComponent<SphereCollider>();

	rb->AddForce(Vector3D(1000.0f * (((double)rand() / (RAND_MAX)) - .5f), 500, -5000));

	_entities.push_back(entity);
}


