#pragma once
#include "weird-engine/Utils.h"
#include "weird-engine/Input.h"
#include "weird-engine/SceneManager.h"
#include "weird-renderer/Renderer.h"

extern "C" {
	_declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
}




//int start(const char* projectPath);
//int main() {
//	const char* projectPath = "SampleProject/";
//	start(projectPath);
//}
// 

namespace WeirdEngine
{
	using namespace WeirdRenderer;
	void start(SceneManager& sceneManager)
	{


		// Window resolution
		const unsigned int width = 1200;
		const unsigned int height = 800;

		Renderer renderer(width, height);

		// Scenes
		sceneManager.loadScene(0);


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

			// Load next scene
			if (Input::GetKeyDown(Input::Q))
			{
				sceneManager.loadNextScene();
			}

			auto scene = sceneManager.getCurrentScene();

			// Update scene logic and physics
			scene->update(delta, time);

			// Clear input
			Input::clear();

			// Render scene
			renderer.render(*scene, time);
		}
	}
}
