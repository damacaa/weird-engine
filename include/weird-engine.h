#pragma once

#ifndef ENGINE_PATH
#define ENGINE_PATH "../weird-engine"
#endif // !ENGINE_PATH

#include "weird-engine/Input.h"
#include "weird-engine/SceneManager.h"
#include "weird-engine/Utils.h"
#include "weird-renderer/Renderer.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C"
{
	EXPORT unsigned long NvOptimusEnablement = 0x00000001;
}

// int start(const char* projectPath);
// int main() {
//	const char* projectPath = "SampleProject/";
//	start(projectPath);
// }
//

namespace WeirdEngine
{
	using namespace WeirdRenderer;
	void start(SceneManager& sceneManager)
	{

		// Window resolution
		const unsigned int width = 800;
		const unsigned int height = 800;

		Renderer renderer(width, height);

		// Scenes
		sceneManager.loadScene(0);

		// Time
		double time = SDL_GetTicks() / 1000.0;
		double prevTime = time;
		double delta = 0;
		double timeDiff = 0;
		unsigned int frameCounter = 0;

		bool quit = false;
		while (!quit)
		{
			// --- UNIFIED EVENT LOOP ---

			// --- UPDATE ---
			// // Meassure time
			time = SDL_GetTicks() / 1000.0;
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
			Input::update(renderer.getWindow());

			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_QUIT)
				{
					quit = true;
				}
				else
				{
					Input::handleEvent(event);
				}
			}

			// Load next scene
			if (Input::GetKeyDown(Input::Q))
			{
				sceneManager.loadNextScene();
			}

			auto scene = sceneManager.getCurrentScene();

			// Update scene logic and physics
			scene->update(delta, time);

			// Clear input
			// Input::clear();

			// Render scene
			renderer.render(*scene, time);
		}
	}
}
