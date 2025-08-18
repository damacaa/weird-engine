
#include <iostream>

#include <weird-engine.h>

#include "CollisionHandling.h"
#include "ApparentCircularMotionScene.h"
#include "Classic.h"
#include "DestroyScene.h"
#include "Fire.h"
#include "FireworksScene.h"
#include "ImageScene.h"
#include "Lines.h"
#include "MouseCollisionScene.h"
#include "RopeScene.h"
#include "Text.h"
#include "Water.h"
#include "ShapesCombinations.h"

int main()
{
	SceneManager &sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<ShapeCombinatiosScene>("shapes");
	sceneManager.registerScene<RopeScene>("rope");
	sceneManager.registerScene<MouseCollisionScene>("cursor-collision");
	sceneManager.registerScene<FireScene>("fire");
	sceneManager.registerScene<LinesScene>("lines");

	// sceneManager.registerScene<CollisionHandlingScene>("collision-handling");
	// sceneManager.registerScene<WaterScene>("water");
	// sceneManager.registerScene<FireSceneRayMarching>("fireRayMarching");
	// sceneManager.registerScene<ClassicScene>("classic");
	// sceneManager.registerScene<TextScene>("text");
	// sceneManager.registerScene<DestroyScene>("empty");
	// sceneManager.registerScene<ImageScene>("image");
	// sceneManager.registerScene<FireworksScene>("fireworks");
	// sceneManager.registerScene<ApparentCircularMotionScene>("circle");
	// sceneManager.registerScene<SpaceScene>("space");

	start(sceneManager);
}