#include "SceneManager.h"


SceneManager::~SceneManager()
{
	m_currentScene = nullptr;
}

void SceneManager::Load(std::string projectDir)
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

	m_scenes.push_back(Scene(projectDir.c_str()));
	m_currentScene = &m_scenes[0];
}

Scene& SceneManager::getCurrentScene()
{
	return *m_currentScene;
}

