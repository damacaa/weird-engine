#include <filesystem>
namespace fs = std::filesystem;


#include <cmath>

#include "weird-engine/Input.h"
#include "weird-engine/Scene.h"
#include "weird-renderer/Renderer.h"


int main()
{
	// Render resolution
	const unsigned int width = 1200;
	const unsigned int height = 800;

	Renderer renderer(width, height);
	Scene scene;

	// Time
	double time = glfwGetTime();
	double prevTime = time;
	double delta = 0;
	double timeDiff = 0;
	unsigned int frameCounter = 0;

	// Main while loop
	while (!renderer.checkWindowClosed())
	{
		// Meassure time
		time = glfwGetTime();
		delta = time - prevTime;
		timeDiff += delta;
		prevTime = time;
		frameCounter++;

		if (timeDiff >= 1.0)
		{
			// Creates new title
			std::string FPS = std::to_string(frameCounter / timeDiff);
			std::string ms = std::to_string((timeDiff / frameCounter) * 1000);
			std::string newTitle = FPS + "FPS / " + ms + "ms";
			renderer.setWindowTitle(newTitle.c_str());

			// Resets times and frameCounter
			timeDiff = 0;
			frameCounter = 0;
		}

		// Capture window input
		Input::update(renderer.getWindow(), width, height);

		// Update scene logic and physics
		scene.update(delta, time);

		// Clear input
		Input::clear();

		// Render scene
		renderer.render(scene, time);
	}

	return 0;
}

