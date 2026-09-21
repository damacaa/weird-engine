#include <weird-engine.h>

#include "AquariumScene.h"
#include "CollisionHandling.h"
#include "DestroyScene.h"
#include "ImageScene.h"
#include "LifeScene.h"
#include "MouseCollisionScene.h"
#include "MusicScene.h"
#include "RopeScene.h"
#include "ShapesCombinations.h"
#include "UiScene.h"
#include "WalkScene.h"

#include "globals.h"
#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();

	sceneManager.registerScene<ShapeCombinationsScene>("shapes");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MusicScene>("music");
	sceneManager.registerScene<LifeScene>("life");
	sceneManager.registerScene<WalkScene>("walk");
	// sceneManager.registerScene<UiScene>("ui"); // TODO: fix AI slop
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
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

	PhysicsSettings physicsSettings{};

	WeirdAudio::AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings, argc, argv);
}