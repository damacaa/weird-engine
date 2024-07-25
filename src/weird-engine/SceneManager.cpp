#include "SceneManager.h"




SceneManager::~SceneManager()
{
	m_currentScene = nullptr;
}

void SceneManager::loadProject(std::string projectDir)
{
	// Find .weird file
	/*auto files = getAllFilesWithExtension(projectDir, "weird");

	for (const auto& file : files) {
		std::cout << file << std::endl;
	}*/

	json projectFile;
	std::string content = get_file_contents((projectDir + "project.weird").c_str());

	projectFile = json::parse(content);
	std::cout << projectFile["Name"] << std::endl;


	// Check if "Scenes" key exists and is an array
	if (projectFile.contains("Scenes") && projectFile["Scenes"].is_array()) {
		for (const auto& scene : projectFile["Scenes"]) {
			m_scenes.push_back(projectDir + scene.get<std::string>());
		}
	}
	else {
		std::cerr << "The 'Scenes' key is missing or is not an array." << std::endl;
	}


	//m_scenes.push_back("{}");
	loadScene(0);
}

Scene& SceneManager::getCurrentScene()
{
	if (m_nextScene != -1) {
		if (m_currentScene)
			delete m_currentScene;

		m_currentScene = new Scene(m_scenes[m_nextScene].c_str());
		m_currentSceneIdx = m_nextScene;
		m_nextScene = -1;
	}

	return *m_currentScene;
}

void SceneManager::loadScene(int idx)
{
	m_nextScene = idx;
}

void SceneManager::loadNextScene()
{
	m_nextScene = (m_currentSceneIdx + 1) % m_scenes.size();
}


