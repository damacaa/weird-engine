#include "ClusterEditor.h"
#include <weird-engine.h>

using namespace WeirdEngine;

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main(int argc, char* argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		std::string_view arg(argv[i]);
		if (!arg.starts_with("-"))
		{
			g_initialClusterPath = argv[i];
			break;
		}
	}

	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<ClusterEditor>("cluster-editor");

	DisplaySettings displaySettings{};
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.windowTitle = "Weird Engine - Cluster Editor";
	displaySettings.worldSmoothFactor = 0.05f;

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
	return 0;
}
