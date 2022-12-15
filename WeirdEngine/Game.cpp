#include "Game.h"
#include "ECS.h"

void Game::Init()
{
	vector<Entity*> entities;

	Entity* camera = new Entity();
	camera->AddComponent<Camera>();
	camera->Transform_->postition = Vector3D(0, 0, -10);
	camera->Transform_->Rotation = Quaternion(Vector3D(0, 1, 0), 180);
	camera->Name = "Camera";
	entities.push_back(camera);

	Quaternion q = Quaternion(Vector3D(0, 1, 0), 90);
	Vector3D euler = q.ToEuler();

	Entity* floor = new Entity();
	{
		//floor->AddComponent<RigidBody>();
		//floor->AddComponent<BoxCollider>();
		auto renderer = floor->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		renderer->_color = Color(1, 1, 1);
		floor->Transform_->postition = Vector3D(0, -4, 0);
		floor->Transform_->Rotation = Quaternion();
		floor->Transform_->scale = Vector3D(100, 1, 100);
		floor->Name = "Floor";
		entities.push_back(floor);
	}

	/*Entity* ball1 = new Entity();
	{
		ball1->AddComponent<RigidBody>();
		ball1->AddComponent<SphereCollider>();
		auto renderer = ball1->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
		renderer->_color = Color(1, 0, 0);
		ball1->Transform_->postition = Vector3D(.5, 0, 0);
		ball1->Transform_->eulerRotation = Vector3D(0, 0, 0);
		ball1->Transform_->scale = Vector3D(2, 2, 2);
		ball1->Name = "Ball_1";
		entities.push_back(ball1);
	}

	Entity* ball2 = new Entity();
	{
		ball2->AddComponent<RigidBody>();
		ball2->AddComponent<SphereCollider>();
		auto renderer = ball2->AddComponent<PrimitiveRenderer>();
		renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
		renderer->_color = Color(1, 0, 0);
		ball2->Transform_->postition = Vector3D(-1, 3, 1);
		ball2->Transform_->eulerRotation = Vector3D(0, 0, 0);
		ball2->Transform_->scale = Vector3D(4, 4, 4);
		ball2->Name = "Ball_2";
		entities.push_back(ball2);
	}*/

	/* {
		Entity* rb = new Entity();
		rb->AddComponent<RigidBody>();
		rb->AddComponent<SphereCollider>();
		auto renderer = rb->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(1, 0, 0);
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		rb->Transform_->postition = Vector3D(0, 0, 0);
		rb->Transform_->Rotation = Quaternion();
		rb->Transform_->scale = Vector3D(1, 1, 1) * (1 + 5 * ((double)rand() / (RAND_MAX)));
		rb->Name = "rigidbody";
		entities.push_back(rb);
	}*/

	srand((unsigned)time(NULL));
	for (size_t i = 0; i < 20; i++)
	{
		Entity* ball = new Entity();

		ball->AddComponent<RigidBody>();
		ball->AddComponent<SphereCollider>();
		auto renderer = ball->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(1, 0, 0);
		renderer->_primitive = PrimitiveRenderer::Primitive::Sphere;
		ball->Transform_->postition = Vector3D(
			0.1f * (((float)rand() / (RAND_MAX)) - 0.5f),
			100.0f * (((float)rand() / (RAND_MAX))),
			0.1f * (((float)rand() / (RAND_MAX))) - 0.5f);

		ball->Transform_->Rotation = Quaternion();
		ball->Transform_->scale = Vector3D(1, 1, 1) * (1 + 5 * ((double)rand() / (RAND_MAX)));
		ball->Name = "Ball_";
		entities.push_back(ball);
	}

	//auto renderer = box->GetComponent<PrimitiveRenderer>();
	//renderer->_primitive = PrimitiveRenderer::Primitive::Octahedron;

	/*Entity* box2 = new Entity();
	{
		auto renderer = box2->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(0, 1, 0);
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		box2->Transform_->postition = Vector3D(1, 1, 0);
		box2->Transform_->eulerRotation = Vector3D(0, 45, 0);
		box2->Transform_->scale = Vector3D(1, 1, 1);
		box2->Transform_->SetParent(box->Transform_);
		box2->name = "Box2";
		entities.push_back(box2);
	}*/


	Scene* mainScene = new(nothrow) Scene(entities);
	this->scenes.push_back(mainScene);

	this->activeScene = mainScene;

}

void Game::Render()
{
	this->activeScene->Render();
}

void Game::Update()
{
	auto time = (duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - this->initialMilliseconds.count());
	auto delta = time - this->lastUpdatedTime;
	this->lastUpdatedTime = time;

	_delay += delta;
	while (_delay > UPDATE_PERIOD)
	{
		Time::FixedDeltaTime = UPDATE_PERIOD / 1000.f;
		this->activeScene->FixedUpdate();
		_delay -= UPDATE_PERIOD;
	}

	Time::DeltaTime = delta / 1000.f;
	this->activeScene->Update();
}

void Game::ProcessKeyPressed(unsigned char key, int px, int py)
{

	if (key == 27)
	{
		cout << "Exit Game" << endl;
		glutDestroyWindow(1);
	}
}

void Game::ProcessKeyReleased(unsigned char key, int px, int py)
{

}

void Game::ProcessMouseMovement(int x, int y)
{

}

void Game::ProcessMouseClick(int button, int state, int x, int y)
{
	if (state == 0)
	{

	}
}