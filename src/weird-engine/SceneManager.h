#pragma once
#include "Scene.h"
#include <vector>
#include <json/json.h>

using json = nlohmann::json;


class SceneManager
{
private:
	Scene* m_currentScene;
	std::vector<std::string> m_scenes;


public:
	~SceneManager();

	void loadProject(std::string projectDir);

	Scene& getCurrentScene();

};

