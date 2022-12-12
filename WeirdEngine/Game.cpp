#include "Game.h"
#include "ECS.h"

void Game::Init()
{
	vector<Entity*> entities;

	Entity* camera = new Entity();
	camera->AddComponent<Camera>();
	camera->Transform_->postition = Vector3D(0, 0, -10);
	camera->Transform_->eulerRotation = Vector3D(0, 180, 0);
	camera->name = "Camera";
	entities.push_back(camera);

	Entity* box = new Entity();
	{
		box->AddComponent<RigidBody>();
		box->AddComponent<Collider>();
		auto renderer = box->AddComponent<PrimitiveRenderer>();
		renderer->_color = Color(1, 0, 0);
		renderer->_primitive = PrimitiveRenderer::Primitive::Cube;
		box->Transform_->postition = Vector3D(0, 0, 0);
		box->Transform_->eulerRotation = Vector3D(0, 0, 0);
		box->Transform_->scale = Vector3D(1, 1, 1);
		box->name = "Box";
		entities.push_back(box);
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