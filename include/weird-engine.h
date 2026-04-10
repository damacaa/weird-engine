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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace WeirdEngine
{

	using namespace WeirdRenderer;

	namespace Detail
	{
		inline SDL_Window* gWindowHandle = nullptr;

		struct RuntimeContext
		{
			SceneManager& sceneManager;
			Renderer& renderer;
			AudioEngine& audioEngine;

			double time = 0.0;
			double prevTime = 0.0;
			double delta = 0.0;
			double timeDiff = 0.0;
			unsigned int frameCounter = 0;
			bool quit = false;
		};

		inline void runFrame(RuntimeContext& ctx)
		{
			// Measure time
			ctx.time = static_cast<double>(SDL_GetTicks()) / 1000.0;
			ctx.delta = ctx.time - ctx.prevTime;
			ctx.timeDiff += ctx.delta;
			ctx.prevTime = ctx.time;
			ctx.frameCounter++;

#ifndef NDEBUG
			if (ctx.timeDiff >= 1.0)
			{
				// Creates new title
				std::string FPS = std::to_string(ctx.frameCounter / ctx.timeDiff);
				std::string ms = std::to_string((ctx.timeDiff / ctx.frameCounter) * 1000);
				std::string newTitle = FPS + "FPS / " + ms + "ms";
				ctx.renderer.setWindowTitle(newTitle.c_str());

				// Resets times and frameCounter
				ctx.timeDiff = 0;
				ctx.frameCounter = 0;
			}
#endif

			// Capture window input
			Input::update(ctx.renderer.getWindow());

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
					ctx.quit = true;
				}
				else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
				{
					int newWidth = event.window.data1;
					int newHeight = event.window.data2;

					std::cout << "Window resized to: " << newWidth << "x" << newHeight << std::endl;

					ctx.renderer.setWindowSize(newWidth, newHeight);
					newResolution = true;
				}
				else
				{
					Input::handleEvent(event);
				}
			}

			auto scene = ctx.sceneManager.getCurrentScene();

			// Update scene logic and physics
			scene->update(ctx.delta, ctx.time);

			// Audio
			{
				PROFILE_SCOPE("Audio Update");
				ctx.audioEngine.listen(*scene);
			}

			// Render scene
			if (newResolution)
				scene->forceShaderRefresh();

			ctx.renderer.render(*scene, ctx.time, ctx.delta);

			// Load next scene if requested
			if (scene->isSceneComplete())
			{
				const auto& nextScene = scene->getNextScene();
				if (!nextScene.empty())
				{
					ctx.sceneManager.loadScene(nextScene);
				}
				else
				{
					ctx.sceneManager.loadNextScene();
				}
			}
		}

#ifdef __EMSCRIPTEN__
		inline RuntimeContext* gRuntimeContext = nullptr;

		inline void runFrameEmscripten()
		{
			if (gRuntimeContext == nullptr)
			{
				return;
			}

			runFrame(*gRuntimeContext);

			if (gRuntimeContext->quit)
			{
				std::cout << "Quitting..." << std::endl;
				delete gRuntimeContext;
				gRuntimeContext = nullptr;
				emscripten_cancel_main_loop();
			}
		}
#endif
	} // namespace Detail

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

		AudioEngine& audioEngine = AudioEngine::getInstance();
		audioEngine.init(audioSettings);

		SDLInitializer m_sdlInitializer(displaySettings, Detail::gWindowHandle, audioEngine);
		Renderer renderer(displaySettings, Detail::gWindowHandle);

		// Scenes
		sceneManager.loadScene(0);

		// Time
		Detail::RuntimeContext runtimeContext{sceneManager, renderer, audioEngine};
		runtimeContext.time = static_cast<double>(SDL_GetTicks()) / 1000.0;
		runtimeContext.prevTime = runtimeContext.time;

#ifdef __EMSCRIPTEN__
		Detail::gRuntimeContext = new Detail::RuntimeContext(runtimeContext);
		emscripten_set_main_loop(Detail::runFrameEmscripten, 0, 1);
#else
		while (!runtimeContext.quit)
		{
			Detail::runFrame(runtimeContext);
		}

		std::cout << "Quitting..." << std::endl;
#endif
		// audioEngine.close();
	}
} // namespace WeirdEngine
