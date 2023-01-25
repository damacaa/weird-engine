#pragma once
#pragma region
#pragma endregion

#include "ECS.h"
#include "Scene.h"

#include <chrono>
#include <iostream>

#include "DebugHelper.h"

using namespace std::chrono;
using namespace std;

class Game
{
private:

	const double UPDATE_PERIOD = 10; // ms tiempo mundo real
	const double MAX_FIXED_UPDATES_PER_FRAME = 2;

	milliseconds m_initialMilliseconds;
	long long m_lastUpdatedTime;
	long long m_delay = 0;

	std::shared_ptr<Scene> m_activeScene;
	vector<Scene*> m_scenes;

	DebugHelper m_debugHelper;

public:
	Game() : m_activeScene(nullptr),
		m_initialMilliseconds(duration_cast<milliseconds>(system_clock::now().time_since_epoch())),
		m_lastUpdatedTime(0) {

		m_debugHelper = DebugHelper();

	};

	~Game() {
		for (auto it = begin(m_scenes); it != end(m_scenes); ++it)
		{
			delete *it;
		}
	};

	void Init();
	void Render();
	void Update();
	void ProcessKeyPressed(unsigned char key, int px, int py);
	void ProcessKeyReleased(unsigned char key, int px, int py);
	void ProcessMouseMovement(int x, int y);
	void ProcessMouseClick(int button, int state, int x, int y);
};

