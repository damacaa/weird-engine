#include <weird-engine.h>
#include "MoleculeEditor.h"

using namespace WeirdEngine;

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<MoleculeEditor>("molecule-editor");

	DisplaySettings displaySettings{};
	displaySettings.width = 640;
	displaySettings.height = 480;
	displaySettings.fullscreen = false;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
	return 0;
}
