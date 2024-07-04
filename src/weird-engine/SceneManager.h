#pragma once
#include "Scene.h"
#include <vector>
#include <json/json.h>

using json = nlohmann::json;


class SceneManager
{
private:
	Scene* m_currentScene;
	std::vector<Scene> m_scenes;


public:
	~SceneManager();

	void Load(std::string projectDir);

	Scene& getCurrentScene();

};

