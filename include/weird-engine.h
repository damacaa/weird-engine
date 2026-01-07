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

		SDL_Window* window;
		AudioEngine audioEngine;
		SDLInitializer m_sdlInitializer(width, height, window, audioEngine);
		Renderer renderer(width, height, window);

		// audioEngine.loadSound(SHADERS_PATH "sample.wav");

		// Scenes
		sceneManager.loadScene(0);

		// Time
		double time = static_cast<double>(SDL_GetTicks()) / 1000.0;
		double prevTime = time;
		double delta = 0;
		double timeDiff = 0;
		unsigned int frameCounter = 0;

		bool quit = false;
		while (!quit)
		{
			// Measure time
			time = static_cast<double>(SDL_GetTicks()) / 1000.0;
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

			bool newResolution = false;
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_QUIT)
				{
					quit = true;
				}
				else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {

					// OPTION A: Get size directly from the event data
					int newWidth = event.window.data1;
					int newHeight = event.window.data2;

					// OPTION B (Safer): Query the window explicitly to be sure
					// SDL_GetWindowSizeInPixels(window, &newWidth, &newHeight);

					std::cout << "Window resized to: " << newWidth << "x" << newHeight << std::endl;

					renderer.setWindowSize(newWidth, newHeight);
					newResolution = true;
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

			// Audio
			audioEngine.listen(*scene);

			// Render scene
			if (newResolution)
				scene->forceShaderRefresh();
			renderer.render(*scene, time, delta);
		}

		std::cout << "Quitting..." << std::endl;
		// audioEngine.close();
	}
}
