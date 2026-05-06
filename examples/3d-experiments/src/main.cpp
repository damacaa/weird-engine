
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

	displaySettings.colorPalette[DisplaySettings::Orange].a = 1.0f;

	displaySettings.materialDataPalette[DisplaySettings::Black].roughness = 0.0f;
	displaySettings.materialDataPalette[DisplaySettings::Black].metallic = 1.0f;

	displaySettings.materialDataPalette[DisplaySettings::White].roughness = 1.0f;
	displaySettings.materialDataPalette[DisplaySettings::White].metallic = 0.0f;

		displaySettings.materialDataPalette[DisplaySettings::Red].roughness = 1.0f;
	displaySettings.materialDataPalette[DisplaySettings::Red].metallic = 0.0f;

		displaySettings.materialDataPalette[DisplaySettings::Green].roughness = 1.0f;
	displaySettings.materialDataPalette[DisplaySettings::Green].metallic = 0.0f;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}