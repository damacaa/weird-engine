#include "weird-engine/SceneManager.h"

#include <algorithm>

namespace WeirdEngine
{
	SceneManager::SceneManager() {}

	SceneManager::~SceneManager() {}

	Scene* SceneManager::getCurrentScene()
	{
		if (currentSceneIdx != targetSceneIdx)
		{
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
		if (sceneFactories.find(sceneName) == sceneFactories.end())
			return;

		// Keep the current/target indices in sync so a later loadNextScene()
		// cycles from the scene that is actually loaded.
		auto it = std::find(names.begin(), names.end(), sceneName);
		if (it != names.end())
		{
			currentSceneIdx = static_cast<int>(it - names.begin());
			targetSceneIdx = currentSceneIdx;
		}

		if (currentScene)
		{
			// Main-thread cleanup hook. The physics thread may still be
			// stepping, so only touch ECS/sim state from the main thread here.
			currentScene->destroy();
		}

		currentScene = nullptr;
		currentScene = sceneFactories[sceneName](); // Instantiate the scene
		currentScene->m_services.resources().setAssetsBasePath(m_assetsPath);
		currentScene->start();
#ifndef NDEBUG
		WeirdEngine::Logger::log("Changed to " + sceneName + " scene");
#endif
	}

	void SceneManager::loadScene(int idx)
	{
		currentSceneIdx = idx;
		targetSceneIdx = idx;
		loadScene(names[idx]);
	}

} // namespace WeirdEngine
