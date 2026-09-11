
#include <iostream>

#include <weird-engine.h>

#include "Classic.h"
#include "CornellBox.h"
#include "MaterialShowcase.h"

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<CornellBox>("cornell_box");
	sceneManager.registerScene<MaterialShowcaseScene>("material_showcase");
	sceneManager.registerScene<ClassicScene>("classic");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.internalResolutionScale = 1.0f;
	displaySettings.worldSmoothFactor = 0.0f;
	displaySettings.vSyncEnabled = true;

	displaySettings.enableDithering = true;
	displaySettings.ditheringColorCount = 4;
	displaySettings.ditheringSpread = 0.3f;

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = true;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}