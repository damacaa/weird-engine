
#include <iostream>

#include <weird-engine.h>

#include "CollisionHandling.h"
#include "DestroyScene.h"
#include "ImageScene.h"
#include "MouseCollisionScene.h"
#include "RopeScene.h"
#include "ShapesCombinations.h"

#include "globals.h"
#include "weird-renderer/core/Display.h"

WeirdEngine::vec3 g_cameraPositon = vec3(15.0f, 7.5f, 35.0f);

int main()
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<ShapeCombinatiosScene>("shapes");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<ImageScene>("image");
	sceneManager.registerScene<CollisionHandlingScene>("collision-handling");
	sceneManager.registerScene<DestroyScene>("destroy-test");

	DisplaySettings displaySettings{};
	displaySettings.width = 800;
	displaySettings.height = 800;
	displaySettings.fullscreen = false;
	displaySettings.colorPalette[DisplaySettings::Yellow].a = 0.25f;

	WeirdEngine::PhysicsSettings physicsSettings{};
	physicsSettings.gravity = -10.0f;
	physicsSettings.damping = 0.001f;
	physicsSettings.simulationFrequency = 250.0f;
	physicsSettings.relaxationSteps = 10;

	WeirdEngine::WeirdRenderer::AudioSettings audioSettings{};
	audioSettings.mute = false;

	start(sceneManager, displaySettings, physicsSettings, audioSettings);
}