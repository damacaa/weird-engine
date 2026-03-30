#include "weird-engine/SceneManager.h"


namespace WeirdEngine
{
	SceneManager::SceneManager()
	{

	}

	SceneManager::~SceneManager()
	{

	}

	Scene* SceneManager::getCurrentScene()
	{
		if (currentSceneIdx != targetSceneIdx) {
			loadScene(targetSceneIdx);
		}

		return currentScene.get();
	}

	void SceneManager::loadNextScene()
	{
		targetSceneIdx = (currentSceneIdx + 1) % sceneFactories.size();
	}

	void SceneManager::loadScene(const std::string& sceneName)
	{
		if (sceneFactories.find(sceneName) != sceneFactories.end()) {
			currentScene = nullptr;
			currentScene = sceneFactories[sceneName](); // Instantiate the scene
			currentScene->start();
			std::cout << "Changed to " << sceneName << " scene\n";
		}
	}

	void SceneManager::loadScene(int idx)
	{
		currentSceneIdx = idx;
		targetSceneIdx = idx;
		loadScene(names[idx]);
	}

	std::map<std::string, Entity> SceneManager::loadSceneFromFile(const std::string& sceneName, const std::string& filePath)
	{
		if (sceneFactories.find(sceneName) != sceneFactories.end()) {
			currentScene = nullptr;
			currentScene = sceneFactories[sceneName]();
			currentScene->startFromFile(filePath);
			std::cout << "Changed to " << sceneName << " scene (from file: " << filePath << ")\n";
			return currentScene->getTags();
		}
		return {};
	}


}


