
#include <weird-engine.h>

#include "AquariumScene.h"
#include "CollisionHandling.h"
#include "DestroyScene.h"
#include "ImageScene.h"
#include "LifeScene.h"
#include "MouseCollisionScene.h"
#include "RopeScene.h"
#include "ShapesCombinations.h"
#include "TextScene.h"
#include "WalkScene.h"

#include "globals.h"
#include "weird-renderer/core/Display.h"

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();

	sceneManager.registerScene<ShapeCombinatiosScene>("shapes");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<TextScene>("text");
	sceneManager.registerScene<LifeScene>("life");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<DestroyScene>("destroy-test");

	// sceneManager.registerScene<CollisionHandlingScene>("collision-handling");
	// sceneManager.registerScene<ImageScene>("image");
	// sceneManager.registerScene<WalkScene>("walk");
	// sceneManager.registerScene<AquariumScene>("aquarium");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.colorPalette[DisplaySettings::Yellow].a = 0.25f;
	displaySettings.distanceSampleScale = 0.5f;

	PhysicsSettings physicsSettings{};

	AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}