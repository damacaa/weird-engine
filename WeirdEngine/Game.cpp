#include "Game.h"
#include "ECS.h"

void Game::Init()
{
	vector<Entity*> entities;

	Scene* mainScene = new(nothrow) Scene();
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

	Input::Update();
}

void Game::ProcessKeyPressed(unsigned char key, int px, int py)
{
	Input::PressKey(key);

	if (key == 27)
	{
		cout << "Exit Game" << endl;
		glutDestroyWindow(1);
	}
}

void Game::ProcessKeyReleased(unsigned char key, int px, int py)
{
	Input::ReleaseKey(key);
}

void Game::ProcessMouseMovement(int x, int y)
{
	Input::SetMouseXY(x, y);
}

void Game::ProcessMouseClick(int button, int state, int x, int y)
{
	Input::HandleMouseButton(button, state);
}