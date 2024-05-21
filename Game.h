#pragma once
#include "src/weird-engine/Scene.h"
class Game
{
private:
	Scene* m_scenes;
	size_t m_sceneCount;
	unsigned int m_currentSceneIdx;

public:
	void Update(double delta, double time);
};

