#include <iostream>
#include <weird-engine.h>

#include "SdfNodeEditorScene.h"

using namespace WeirdEngine;
using namespace WeirdEngine::Editor;

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<SdfNodeEditorScene>("sdf-node-editor");

	DisplaySettings displaySettings{};
	displaySettings.width = 1280;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.windowTitle = "Weird Engine - SDF Node Editor";

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	WeirdEngine::start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
	return 0;
}
