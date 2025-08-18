#include <iostream> // Only needed for debug output, can remove if unused
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
	}

	void onUpdate() override
	{
	}

	void onRender() override
	{
	}
};

int main()
{
	SceneManager& sceneManager = SceneManager::getInstance();
	sceneManager.registerScene<EmptyScene>("empty");
	start(sceneManager);
}
