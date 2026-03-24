#pragma once
#include "Scene.h"
#include <vector>
#include <json/json.h>

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

		void loadProject(std::string projectDir);

		Scene* getCurrentScene();
		
		void setPhysicsSettings(const PhysicsSettings& settings) { m_physicsSettings = settings; }
		const PhysicsSettings& getPhysicsSettings() const { return m_physicsSettings; }

		static SceneManager& getInstance() {
			static SceneManager* _instance = new SceneManager();
			return *_instance;
		};

		void loadNextScene();

		template <typename T>
		void registerScene(const std::string& sceneName, const std::string& sceneFilePath = "");

		void loadScene(const std::string& sceneName);
		void loadScene(int idx);

	private:
		std::map<std::string, std::function<std::unique_ptr<Scene>()>> sceneFactories;
		std::vector<std::string> names;
		std::unique_ptr<Scene> currentScene;

		int currentSceneIdx = 0;
		int targetSceneIdx = 0;
		PhysicsSettings m_physicsSettings;
	};


	// ChatGPT: Template method declarations and definitions are usually placed in header files.
	// The header files are then included in the source files that use the templates.
	// If the template is only in the static library and the client code doesn’t see its full definition, it won't be able to use it.
	// That's why this is here...
	template<typename T>
	void SceneManager::registerScene(const std::string& sceneName, const std::string& sceneFilePath)
	{
		static_assert(std::is_base_of<Scene, T>::value, "T must derive from Scene");

		// TODO: check ECS for a similar
		names.push_back(sceneName);
		sceneFactories[sceneName] = [this, sceneFilePath]() {
			auto scene = std::make_unique<T>(m_physicsSettings);
			if (!sceneFilePath.empty())
			{
				scene->setSceneFilePath(sceneFilePath);
			}
			return scene;
			};
	}

}