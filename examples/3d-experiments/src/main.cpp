
#include <iostream>

#include <weird-engine.h>

#include "Classic.h"
#include "Fire.h"
#include "Lines.h"
#include "GlobalIllumination.h"
#include "CornellBox.h"

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<CornellBox>("cornell_box");
	sceneManager.registerScene<GlobalIlluminationScene>("global_illumination");
	sceneManager.registerScene<ClassicScene>("classic");
	sceneManager.registerScene<FireScene>("fire");
	sceneManager.registerScene<LinesScene>("lines");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.internalResolutionScale = 1.0f;
	displaySettings.worldSmoothFactor = 0.0f;
	displaySettings.vSyncEnabled = true;

	// displaySettings.colorPalette[DisplaySettings::Red].r = 2.0f;
	// displaySettings.colorPalette[DisplaySettings::Red].a = .25f;

	displaySettings.enableDithering = false;

	displaySettings.colorPalette[DisplaySettings::Orange].a = 0.5f;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}