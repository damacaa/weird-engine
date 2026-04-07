
#include <iostream>

#include <weird-engine.h>

#include "Classic.h"
#include "Fire.h"
#include "Lines.h"

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<ClassicScene>("classic");
	sceneManager.registerScene<FireScene>("fire");
	sceneManager.registerScene<FireSceneRayMarching>("fireRayMarching");
	sceneManager.registerScene<LinesScene>("lines");

	DisplaySettings settings{};
	settings.width = 800;
	settings.height = 800;
	settings.fullscreen = false;

	start(sceneManager, settings, {}, {}, argc, argv);
}