#include <weird-engine.h> // Main engine include

using namespace WeirdEngine;

// Starter template for a new game. Register scenes in main() and the engine
// takes care of the rest. Override the callbacks you need:
//   onCreate            - once, before the physics thread starts
//   onStart(registry[, tags])- after the ECS is ready and the physics thread runs
//   onUpdate(dt, registry)   - game logic, once per frame (pure virtual)
//   onRender(target)    - extra 3D rendering
//   onImGuiRender       - debug UI
//   onCollision / onShapeCollision             - physics thread, no ECS access
//   onEntityCollision / onEntityShapeCollision - main thread, ECS safe
//   onDestroy           - before the scene is replaced during a transition
class EmptyScene : public Scene2D
{
private:
	void onStart(Registry& registry, ServiceProvider& services) override {}

	void onUpdate(Registry& registry, ServiceProvider& services) override {}
};

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<EmptyScene>("empty");
	start(sceneManager, {}, {}, {}, argc, argv);
}
