#include "SceneEditor.h"
#include <weird-engine.h>

using namespace WeirdEngine;

WeirdEngine::vec3 g_cameraPositon = vec3(0.0f, 0.0f, 35.0f);

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<SceneEditor>("scene-editor");

	DisplaySettings displaySettings{};
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.windowTitle = "Weird Engine - Scene Editor";

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
	return 0;
}
