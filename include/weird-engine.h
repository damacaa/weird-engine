#pragma once

#ifndef ENGINE_PATH
#define ENGINE_PATH "../weird-engine"
#endif // !ENGINE_PATH

#include <cstdlib>
#include <iostream>

#include <SDL3/SDL.h>
#include <SDL3/SDL_timer.h>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#endif

#include "weird-engine/Input.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/Logger.h"
#include "weird-engine/SceneManager.h"
#include "weird-renderer/core/Renderer.h"
#include "weird-renderer/core/SDLInitializer.h"

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
extern "C"
{
	EXPORT unsigned long NvOptimusEnablement = 0x00000001;
	EXPORT int AmdPowerXpressRequestHighPerformance = 1;
}
#else
#define EXPORT
#endif

#ifndef ASSETS_PATH
#define ASSETS_PATH
#endif // !ASSETS_PATH

#ifndef SHADERS_PATH
#define SHADERS_PATH
#endif // !SHADERS_PATH


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
		inline SDL_Window* g_windowHandle = nullptr;

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

			// Testing/diagnostics support
			double startTime = 0.0;
			double autoQuitAt = -1.0; // absolute time to quit, -1 disables
			unsigned long long totalFrames = 0;
		};

		inline void runFrame(RuntimeContext& ctx)
		{
			// Measure time
			ctx.time = static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
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

			WeirdEngine::Profiler::get().update();

			// Test hooks: auto-quit and periodic heartbeat logging (useful on
			// headless devices where log.txt is the only output).
			ctx.totalFrames++;
			if (ctx.totalFrames % 120 == 0)
			{
				std::cout << "[WeirdEngine] frame " << ctx.totalFrames
						  << " (t=" << (ctx.time - ctx.startTime) << "s)" << std::endl;
			}
			if (ctx.autoQuitAt > 0.0 && ctx.time >= ctx.autoQuitAt)
			{
				std::cout << "[WeirdEngine] Auto-quit time reached, exiting cleanly." << std::endl;
				ctx.quit = true;
			}

			PROFILE_SCOPE("Frame");

			bool newResolution = false;
			// Capture window input and events
			{
				PROFILE_SCOPE("Input events");
				Input::update(ctx.renderer.getWindow());

#ifndef WEIRD_DISABLE_IMGUI
				// Suppress game input when ImGui has focus
				{
					const ImGuiIO& io = ImGui::GetIO();
					if (io.WantCaptureMouse)
						Input::suppressMouseInput();
					if (io.WantCaptureKeyboard)
						Input::suppressKeyboardInput();
				}
#endif
				
				SDL_Event event;
				while (SDL_PollEvent(&event))
				{
#ifndef WEIRD_DISABLE_IMGUI
					ImGui_ImplSDL3_ProcessEvent(&event);
#endif
					
					if (event.type == SDL_EVENT_QUIT)
					{
						ctx.quit = true;
					}
					else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
					{
						int newWidth = event.window.data1;
						int newHeight = event.window.data2;
						
						WeirdEngine::Logger::log("Window resized to: " + std::to_string(newWidth) + "x" + std::to_string(newHeight));
						
						ctx.renderer.setWindowSize(newWidth, newHeight);
						newResolution = true;
					}
					else
					{
						Input::handleEvent(event);
					}
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
		// Wrapper struct to own heap-allocated resources for Emscripten
		struct EmscriptenRuntimeEnvironment
		{
			SDLInitializer* sdlInitializer = nullptr;
			Renderer* renderer = nullptr;
			RuntimeContext* runtimeContext = nullptr;
		};

		inline EmscriptenRuntimeEnvironment* g_emscriptenEnv = nullptr;

		inline void runFrameEmscripten()
		{
			if (g_emscriptenEnv == nullptr || g_emscriptenEnv->runtimeContext == nullptr)
			{
				return;
			}

			runFrame(*g_emscriptenEnv->runtimeContext);

			if (g_emscriptenEnv->runtimeContext->quit)
			{
				WeirdEngine::Logger::Log("Quitting...");
				// Clean up heap-allocated resources
				if (g_emscriptenEnv->runtimeContext != nullptr)
				{
					delete g_emscriptenEnv->runtimeContext;
					g_emscriptenEnv->runtimeContext = nullptr;
				}
				if (g_emscriptenEnv->renderer != nullptr)
				{
					delete g_emscriptenEnv->renderer;
					g_emscriptenEnv->renderer = nullptr;
				}
				if (g_emscriptenEnv->sdlInitializer != nullptr)
				{
					delete g_emscriptenEnv->sdlInitializer;
					g_emscriptenEnv->sdlInitializer = nullptr;
				}
				delete g_emscriptenEnv;
				g_emscriptenEnv = nullptr;
				emscripten_cancel_main_loop();
			}
		}
#endif
	} // namespace Detail

	void start(SceneManager& sceneManager, DisplaySettings displaySettings = {}, PhysicsSettings physicsSettings = {},
			   AudioSettings audioSettings = {}, int argc = 0, char** argv = nullptr)
	{
		WeirdEngine::Logger::log("Starting Weird Engine...");

		std::string startupScene;
		for (int i = 1; i < argc; ++i)
		{
			const std::string_view arg(argv[i]);
			if (arg == "--fullscreen" || arg == "-f")
			{
				displaySettings.fullscreen = true;
			}else if ((arg == "--scene" || arg == "-s") && i + 1 < argc)
			{
				startupScene = argv[++i]; // Get the next argument as the scene name
				WeirdEngine::Logger::log("Startup scene set to: " + startupScene);
			}
		}

		


		sceneManager.setPhysicsSettings(physicsSettings);

		AudioEngine& audioEngine = AudioEngine::getInstance();
		audioEngine.init(audioSettings);

#ifdef __EMSCRIPTEN__
		// In Emscripten, allocate all resources on the heap to prevent stack unwinding issues
		Detail::g_emscriptenEnv = new Detail::EmscriptenRuntimeEnvironment();
		
		// Create SDLInitializer and Renderer on the heap
		try
		{
			Detail::g_emscriptenEnv->sdlInitializer = new SDLInitializer(displaySettings, Detail::g_windowHandle, audioEngine);
			Detail::g_emscriptenEnv->renderer = new Renderer(displaySettings, Detail::g_windowHandle);
		}
		catch (...)
		{
			WeirdEngine::Logger::error("Failed to initialize SDL/Renderer");
			if (Detail::g_emscriptenEnv->sdlInitializer != nullptr)
			{
				delete Detail::g_emscriptenEnv->sdlInitializer;
			}
			if (Detail::g_emscriptenEnv->renderer != nullptr)
			{
				delete Detail::g_emscriptenEnv->renderer;
			}
			delete Detail::g_emscriptenEnv;
			Detail::g_emscriptenEnv = nullptr;
			return;
		}

		// Scenes
		if (!startupScene.empty())
		{
			sceneManager.loadScene(startupScene);
		}
		else
		{
			sceneManager.loadScene(0);
		}

		// Time - create RuntimeContext with references to heap-allocated objects
		Detail::g_emscriptenEnv->runtimeContext = new Detail::RuntimeContext{
			sceneManager,
			*Detail::g_emscriptenEnv->renderer,
			audioEngine
		};
		Detail::g_emscriptenEnv->runtimeContext->time = static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
		Detail::g_emscriptenEnv->runtimeContext->prevTime = Detail::g_emscriptenEnv->runtimeContext->time;
		Detail::g_emscriptenEnv->runtimeContext->startTime = Detail::g_emscriptenEnv->runtimeContext->time;

		emscripten_set_main_loop(Detail::runFrameEmscripten, 0, 1);
#else
		SDLInitializer m_sdlInitializer(displaySettings, Detail::g_windowHandle, audioEngine);
		Renderer renderer(displaySettings, Detail::g_windowHandle);

		// Scenes
		if (!startupScene.empty())
		{
			sceneManager.loadScene(startupScene);
		}
		else
		{
			sceneManager.loadScene(0);
		}

		// Time
		Detail::RuntimeContext runtimeContext{sceneManager, renderer, audioEngine};
		runtimeContext.time = static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(SDL_GetPerformanceFrequency());
		runtimeContext.prevTime = runtimeContext.time;
		runtimeContext.startTime = runtimeContext.time;

		// Optional self-termination after N seconds (WEIRD_AUTO_QUIT_SECONDS),
		// so headless test runs always return to the launcher.
		if (const char* aq = SDL_getenv("WEIRD_AUTO_QUIT_SECONDS"))
		{
			double seconds = atof(aq);
			if (seconds > 0.0)
			{
				runtimeContext.autoQuitAt = runtimeContext.time + seconds;
			}
		}

		while (!runtimeContext.quit)
		{
			Detail::runFrame(runtimeContext);
		}

		WeirdEngine::Logger::log("Quitting...");
#endif
		// audioEngine.close();
	}
} // namespace WeirdEngine
