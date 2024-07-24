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

	int m_nextScene = -1;

	SceneManager() {};

public:
	~SceneManager();

	void loadProject(std::string projectDir);

	Scene& getCurrentScene();

	static SceneManager& getInstance() {
		static SceneManager* _instance = new SceneManager();
		return *_instance;
	};

	 void loadScene(int idx);
};

