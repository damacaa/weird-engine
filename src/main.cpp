#include <filesystem>
namespace fs = std::filesystem;

#include "weird-renderer/Model.h"
#include "weird-renderer/Shape.h"
#include "weird-renderer/RenderPlane.h"

#include <cmath>

#include "weird-physics/Simulation.h"
#include "weird-renderer/Renderer.h"
#include "weird-engine/Scene.h"




int main()
{
	// Render resolution
	const unsigned int width = 1200;
	const unsigned int height = 800;

	Renderer renderer(width, height);

	size_t size;
	Shape* data;
	Scene scene;

	// Time
	double time = glfwGetTime();
	double prevTime = time;
	double delta = 0;
	double timeDiff = 0;
	unsigned int frameCounter = 0;

	const double fixedDeltaTime = 1 / 100.0;
	const size_t MAX_STEPS = 1000000;
	double simulationDelay = 0;

	// Main while loop
	while (!renderer.CheckWindowClosed())
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
			renderer.SetWindowTitle(newTitle.c_str());

			// Resets times and frameCounter
			timeDiff = 0;
			frameCounter = 0;
		}


		int steps = 0;
		while (simulationDelay >= fixedDeltaTime && steps < MAX_STEPS)
		{
			//m_simulation.Step((float)fixedDeltaTime);
			simulationDelay -= fixedDeltaTime;
			++steps;
		}

		if (steps >= MAX_STEPS)
			std::cout << "Not enough steps for simulation" << std::endl;


		scene.Update(delta, time);

		// TODO: move somewhere else
		// Handles camera inputs
		scene.m_camera->Inputs(renderer.m_window, delta * 100.0f);
		
		// Render scene
		renderer.Render(scene, time);
	}

	return 0;
}

