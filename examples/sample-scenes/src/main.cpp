
#include <iostream>

#include <weird-engine.h>

#include "CollisionHandling.h"
#include "DestroyScene.h"
#include "ImageScene.h"
#include "LifeScene.h"
#include "MouseCollisionScene.h"
#include "RopeScene.h"
#include "ShapesCombinations.h"
#include "WalkScene.h"

#include "globals.h"
#include "MoleculeEditor.h"
#include "SceneLoadExample.h"
#include "weird-renderer/core/Display.h"

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main(int argc, char* argv[])
{
	// Ensure log output survives a crash when piping to a file (headless devices)
	setvbuf(stdout, nullptr, _IONBF, 0);
	setvbuf(stderr, nullptr, _IONBF, 0);

	std::cout << "[WeirdSamples] Starting up..." << std::endl;

	SceneManager& sceneManager = SceneManager::getInstance();

	sceneManager.registerScene<ShapeCombinatiosScene>("shapes");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<ImageScene>("image");
	sceneManager.registerScene<CollisionHandlingScene>("collision-handling");
	sceneManager.registerScene<DestroyScene>("destroy-test");
	sceneManager.registerScene<SceneLoadExample>("scene-editor", ASSETS_PATH "example.weird");
	sceneManager.registerScene<MoleculeEditor>("molecule-editor");
	sceneManager.registerScene<LifeScene>("life");
	sceneManager.registerScene<WalkScene>("walk");

	DisplaySettings displaySettings{};
	displaySettings.width = 640;
	displaySettings.height = 480;
	displaySettings.fullscreen = true;
	displaySettings.colorPalette[DisplaySettings::Yellow].a = 0.25f;
	displaySettings.distanceSampleScale = 0.5f;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}