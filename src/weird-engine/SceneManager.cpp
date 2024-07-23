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
	m_currentScene = new Scene(m_scenes[0].c_str());
}

Scene& SceneManager::getCurrentScene()
{
	return *m_currentScene;
}

