#include <iostream>		  // Only needed for debug output, can remove if unused
#include <weird-engine.h> // Main engine include

using namespace WeirdEngine;

// Example scene demonstrating how to create a rope of connected circles using springs.
class EmptyScene : public Scene
{
public:
	EmptyScene()
		: Scene()
	{
	}

private:
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching2D;
	}

	void onUpdate(float delta) override {}
	void onCreate() override {}
	void onRender(WeirdRenderer::RenderTarget& renderTarget) override {}
	void onCollision(WeirdEngine::CollisionEvent& event) override {}
	void onShapeCollision(WeirdEngine::ShapeCollisionEvent& event) override {}
	void onDestroy() override {}
};

int main(int argc, char* argv[])
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<EmptyScene>("empty");
	start(sceneManager, {}, {}, {}, argc, argv);
}
