#include "Game.h"
#include "ECS.h"

void Game::Init()
{
	vector<Entity*> entities;

	Scene* mainScene = new(nothrow) Scene();
	this->m_scenes.push_back(mainScene);

	this->m_activeScene = mainScene;

}

void Game::Render()
{
	this->m_activeScene->Render();
}

void Game::Update()
{
	auto time = (duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - this->m_initialMilliseconds.count());
	auto delta = time - this->m_lastUpdatedTime;
	this->m_lastUpdatedTime = time;

	m_delay += delta;
	while (m_delay > UPDATE_PERIOD)
	{
		Time::fixedDeltaTime = UPDATE_PERIOD / 1000.f;
		this->m_activeScene->FixedUpdate();
		m_delay -= UPDATE_PERIOD;
	}

	Time::deltaTime = delta / 1000.f;
	this->m_activeScene->Update();

	Time::currentTime += Time::deltaTime;


	Input::GetInstance().Update();
}

void Game::ProcessKeyPressed(unsigned char key, int px, int py)
{
	Input::GetInstance().PressKey(key);

	if (key == 27)
	{
		cout << "Exit Game" << endl;
		glutDestroyWindow(1);
	}

}

void Game::ProcessKeyReleased(unsigned char key, int px, int py)
{
	Input::GetInstance().ReleaseKey(key);
}

void Game::ProcessMouseMovement(int x, int y)
{
	Input::GetInstance().SetMouseXY(x, y);
}

void Game::ProcessMouseClick(int button, int state, int x, int y)
{
	Input::GetInstance().HandleMouseButton(button, state);
}