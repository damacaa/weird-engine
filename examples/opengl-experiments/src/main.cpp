
#include <iostream>

#include <weird-engine.h>

#include "Fire.h"
#include "Lines.h"
#include "Water.h"

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<FireScene>("fire");
	sceneManager.registerScene<LinesScene>("lines");
	sceneManager.registerScene<WaterScene>("water");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.internalResolutionScale = 1.0f;
	displaySettings.worldSmoothFactor = 0.0f;
	displaySettings.vSyncEnabled = true;

	// displaySettings.colorPalette[DisplaySettings::Red].r = 2.0f;
	// displaySettings.colorPalette[DisplaySettings::Red].a = .25f;

	displaySettings.enableDithering = true;
	displaySettings.ditheringColorCount = 4;
	displaySettings.ditheringSpread = 0.3f;

	displaySettings.colorPalette[DisplaySettings::Orange].a = 1.0f;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = true;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}