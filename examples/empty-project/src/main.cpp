#include <weird-engine.h>

using namespace WeirdEngine;

// Starter template for a new game. Register scenes in main() and the engine
// takes care of the rest.
//
// Scene logic lives in free-function systems registered by the scene
// constructor; registration order is execution order:
//   addStartSystem                - once, after managers/camera/scene file
//   addUpdateSystem               - every frame
//   addImGuiRenderSystem          - debug UI
//   addEntityCollisionSystem      - main-thread body-body collisions
//   addEntityShapeCollisionSystem - main-thread body-shape collisions
//   addDestroySystem              - before the scene is replaced
//
// Legacy virtual callbacks are still supported. See docs/SCENE_AND_ECS.md and
// examples/sample-scenes for full systems examples (UiScene, CollisionHandling, etc.).
class EmptyScene : public Scene2D
{
public:
	EmptyScene()
	{
		// addStartSystem(onStartSystem);
		// addUpdateSystem(onUpdateSystem);
	}
};

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<EmptyScene>("empty");
	start(sceneManager, {}, {}, {}, argc, argv);
}
