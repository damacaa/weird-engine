#include "SceneManager.h"



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
		currentScene = sceneFactories[sceneName](); // Instantiate the scene
		std::cout << "Changed to " << sceneName << " scene\n";
	}
}

void SceneManager::loadScene(int idx)
{
	currentSceneIdx = idx;
	targetSceneIdx = idx;
	loadScene(names[idx]);
}





