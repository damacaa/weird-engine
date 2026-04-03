#pragma once

#ifndef ENGINE_PATH
#define ENGINE_PATH "../weird-engine"
#endif // !ENGINE_PATH

#include "weird-engine/Input.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/SceneManager.h"
#include "weird-renderer/core/Renderer.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C"
{
	EXPORT unsigned long NvOptimusEnablement = 0x00000001;
}

#include "weird-physics/PhysicsSettings.h"
#include "weird-renderer/audio/AudioEngine.h"
#include "weird-renderer/audio/AudioSettings.h"

namespace WeirdEngine
{

	using namespace WeirdRenderer;
	void start(SceneManager& sceneManager, DisplaySettings displaySettings = {}, PhysicsSettings physicsSettings = {},
			   AudioSettings audioSettings = {}, int argc = 0, char** argv = nullptr)
	{
		for (int i = 1; i < argc; ++i)
		{
			const std::string_view arg(argv[i]);
			if (arg == "--fullscreen" || arg == "-f")
			{
				displaySettings.fullscreen = true;
			}
		}

		sceneManager.setPhysicsSettings(physicsSettings);

		SDL_Window* window;
		AudioEngine& audioEngine = AudioEngine::getInstance();
		audioEngine.init(audioSettings);

		SDLInitializer m_sdlInitializer(displaySettings, window, audioEngine);
		Renderer renderer(displaySettings, window);

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

			if (Input::GetKeyDown(Input::T))
			{
				WeirdEngine::Profiler::Get().startRecording();
			}
			WeirdEngine::Profiler::Get().update();

			PROFILE_SCOPE("Frame");

			bool newResolution = false;
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_QUIT)
				{
					quit = true;
				}
				else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
				{
					int newWidth = event.window.data1;
					int newHeight = event.window.data2;

					std::cout << "Window resized to: " << newWidth << "x" << newHeight << std::endl;

					renderer.setWindowSize(newWidth, newHeight);
					newResolution = true;
				}
				else
				{
					Input::handleEvent(event);
				}
			}

			auto scene = sceneManager.getCurrentScene();

			// Update scene logic and physics
			scene->update(delta, time);

			// Audio
			{
				PROFILE_SCOPE("Audio Update");
				audioEngine.listen(*scene);
			}

			// Render scene
			if (newResolution)
				scene->forceShaderRefresh();

			renderer.render(*scene, time, delta);

			// Load next scene if requested
			if (scene->isSceneComplete())
			{
				const auto& nextScene = scene->getNextScene();
				if (!nextScene.empty())
				{
					sceneManager.loadScene(nextScene);
				}
				else
				{
					sceneManager.loadNextScene();
				}
			}
		}

		std::cout << "Quitting..." << std::endl;
		// audioEngine.close();
	}
} // namespace WeirdEngine
