#include "Game.h"
#include "ECS.h"

void Game::Init()
{
	Scene* mainScene = new(nothrow) Scene();
	m_scenes.push_back(mainScene);
	m_activeScene.reset(mainScene);
}

void Game::Render()
{
	this->m_activeScene->Render();
}

void Game::Update()
{
	m_debugHelper.RecordTime("Update");


	auto time = (duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() - this->m_initialMilliseconds.count());
	auto delta = time - this->m_lastUpdatedTime;
	this->m_lastUpdatedTime = time;

	m_delay += delta;
	int fixedUpdates = 0;
	while (m_delay > UPDATE_PERIOD && fixedUpdates < MAX_FIXED_UPDATES_PER_FRAME)
	{
		Time::fixedDeltaTime = UPDATE_PERIOD / 1000.f;
		this->m_activeScene->FixedUpdate();
		m_delay -= UPDATE_PERIOD;
		++fixedUpdates;
	}

	Time::deltaTime = delta / 1000.f;
	this->m_activeScene->Update();

	Time::currentTime += Time::deltaTime;

	Input::Update();

	m_debugHelper.Wait();
}

void Game::ProcessKeyPressed(unsigned char key, int px, int py)
{
	Input::PressKey(key);

	if (key == 27)
	{
		cout << "Exit Game" << endl;
		m_debugHelper.PrintTimes();
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