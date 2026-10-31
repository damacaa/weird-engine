#include <weird-engine.h>

#include "AquariumScene.h"
#include "CollisionHandling.h"
#include "CollisionScene.h"
#include "DestroyScene.h"
#include "ImageScene.h"
#include "LifeScene.h"
#include "MenuScene.h"
#include "MusicScene.h"
#include "PlatformerScene.h"
#include "RopeScene.h"
#include "ShapesCombinations.h"
#include "UiScene.h"

#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();

	sceneManager.registerScene<MenuScene>("menu");
	sceneManager.registerScene<ShapeCombinationsScene>("shapes");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MusicScene>("music");
	sceneManager.registerScene<LifeScene>("life");
	sceneManager.registerScene<PlatformerScene>("platformer");
	// sceneManager.registerScene<UiScene>("ui"); // TODO: fix AI slop
	sceneManager.registerScene<CollisionScene>("collisions");
	sceneManager.registerScene<ImageScene>("image");
	// sceneManager.registerScene<CollisionHandlingScene>("collision-handling");

	// Stress tests / benchmarks (commented out due to runtime shader rebuild overhead)
	// sceneManager.registerScene<DestroyScene>("destroy-test");

	// AI slop
	// sceneManager.registerScene<AquariumScene>("aquarium");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.distanceSampleScale = 0.5f;
	displaySettings.uiSmoothFactor = 10.0f;

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}