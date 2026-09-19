#include <iostream>
#include <weird-engine.h>

#include "ShapeEditorScene.h"

using namespace WeirdEngine;
using namespace WeirdEngine::Editor;

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<ShapeEditorScene>("shape-editor");

	DisplaySettings displaySettings{};
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.windowTitle = "Weird Engine - Shape Editor";
	displaySettings.worldSmoothFactor = 4.0f;

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = false;

	WeirdEngine::start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
	return 0;
}
