#pragma once
#include "Scene.h"
#include <json/json.h>
#include <vector>

#include "../weird-physics/PhysicsSettings.h"
namespace WeirdEngine
{
	using json = nlohmann::json;

	class SceneManager
	{
	private:
		SceneManager();

	public:
		~SceneManager();

		Scene* getCurrentScene();

		void setPhysicsSettings(const PhysicsSettings& settings)
		{
			m_physicsSettings = settings;
		}
		const PhysicsSettings& getPhysicsSettings() const
		{
			return m_physicsSettings;
		}

		void setAssetsPath(const std::string& path)
		{
			m_assetsPath = path;
		}

		static SceneManager& getInstance()
		{
			static SceneManager _instance;
			return _instance;
		};

		void loadNextScene();

		template <typename T> void registerScene(const std::string& sceneName, const std::string& sceneFilePath = "");

		void loadScene(const std::string& sceneName);
		void loadScene(int idx);

	private:
		std::map<std::string, std::function<std::unique_ptr<Scene>()>> sceneFactories;
		std::vector<std::string> names;
		std::unique_ptr<Scene> currentScene;

		int currentSceneIdx = 0;
		int targetSceneIdx = 0;
		PhysicsSettings m_physicsSettings;
		std::string m_assetsPath;
	};

	template <typename T>
	void SceneManager::registerScene(const std::string& sceneName, const std::string& sceneFilePath)
	{
		static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");

		names.push_back(sceneName);
		sceneFactories[sceneName] = [this, sceneFilePath]()
		{
			auto scene = std::make_unique<T>();
			if (!sceneFilePath.empty())
			{
				scene->setSceneFilePath(sceneFilePath);
			}
			return scene;
		};
	}

} // namespace WeirdEngine