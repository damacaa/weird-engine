#include "weird-renderer/core/SDLInitializer.h"

#include <csignal>
#include <iostream>
#include <stdexcept>

#include <glad/glad.h>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#endif

#include "weird-renderer/core/WeirdFBDevEGL.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		void APIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
									  const GLchar* message, const void* userParam)
		{
			fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
					(type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity, message);

			// If it's a high-severity error, break the debugger!
			if (severity == GL_DEBUG_SEVERITY_HIGH)
			{
#ifdef _WIN32
				__debugbreak(); // This acts as a breakpoint
#else
				raise(SIGTRAP);
#endif
			}
		}

		SDLInitializer::SDLInitializer(DisplaySettings& settings, SDL_Window*& window, AudioEngine& audioEngine)
			: m_window(window)
		{
#ifdef WEIRD_USE_FBDEV_EGL
			// Video backend selection: fbdev EGL by default, SDL's own windowing
			// (e.g. kmsdrm) when WEIRD_VIDEO_BACKEND=sdl.
			const char* backend = SDL_getenv("WEIRD_VIDEO_BACKEND");
			m_useFBDevEGL = !(backend && SDL_strcmp(backend, "sdl") == 0);

			if (m_useFBDevEGL)
			{
				SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
			}
#endif
			if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD))
			{
				std::cout << "SDL_Init with audio failed: " << SDL_GetError() << " - retrying without audio"
						  << std::endl;
				if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
				{
					std::string errorMsg = "SDL could not initialize! SDL_Error: ";
					errorMsg += SDL_GetError();
					throw std::runtime_error(errorMsg);
				}
			}
			std::cout << "SDL initialized. Video driver: "
					  << (SDL_GetCurrentVideoDriver() ? SDL_GetCurrentVideoDriver() : "<none>") << std::endl;

			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

			SDL_WindowFlags windowFlags;
			if (m_useFBDevEGL)
			{
				windowFlags = 0;
			}
			else
			{
				windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

				if (settings.fullscreen)
				{
					windowFlags |= SDL_WINDOW_FULLSCREEN;
				}
			}

			window = SDL_CreateWindow(settings.windowTitle.c_str(), settings.width, settings.height, windowFlags);
			if (!window)
			{
				throw std::runtime_error("Failed to create SDL window.");
			}

			if (!m_useFBDevEGL)
			{
				if (!settings.fullscreen)
				{
					SDL_SetWindowPosition(window, settings.x, settings.y);
				}

				int displayIndex = SDL_GetDisplayForWindow(window);
				if (displayIndex < 0)
				{
					SDL_Log("Failed to get display index: %s", SDL_GetError());
					return;
				}

				const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(displayIndex);
				settings.refreshRate = mode->refresh_rate;

				if (settings.fullscreen)
				{
					SDL_GetWindowSizeInPixels(window, reinterpret_cast<int*>(&settings.width),
											  reinterpret_cast<int*>(&settings.height));
				}
			}

			m_window = window;

			if (m_useFBDevEGL)
			{
				int fbWidth, fbHeight;
				if (!InitFBDevEGL(fbWidth, fbHeight))
				{
					throw std::runtime_error("Failed to initialize fbdev EGL");
				}
				m_glContext = nullptr;
				settings.width = fbWidth;
				settings.height = fbHeight;

				if (!gladLoadGLES2Loader((GLADloadproc)GetEGLProcAddress))
				{
					throw std::runtime_error("Failed to initialize GLAD via EGL.");
				}
			}
			else
			{
				m_glContext = SDL_GL_CreateContext(m_window);
				if (!m_glContext)
				{
					throw std::runtime_error("Failed to create OpenGL context.");
				}

				SDL_GL_MakeCurrent(m_window, m_glContext);

				if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
				{
					throw std::runtime_error("Failed to initialize GLAD.");
				}
			}

			std::cout << "GL_SHADING_LANGUAGE_VERSION: " << (const char*)glGetString(GL_SHADING_LANGUAGE_VERSION)
					  << std::endl;

#ifndef WEIRD_DISABLE_IMGUI
			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGui::StyleColorsDark();

			// Neutral gray ImGui theme (pure grays without blue tint)
			ImGuiStyle& imguiStyle = ImGui::GetStyle();
			imguiStyle.Colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.0f);
			imguiStyle.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
			imguiStyle.Colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.98f);
			imguiStyle.Colors[ImGuiCol_Border] = ImVec4(0.24f, 0.24f, 0.24f, 0.70f);
			imguiStyle.Colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.0f);
			imguiStyle.Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
			imguiStyle.Colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.0f);
			imguiStyle.Colors[ImGuiCol_TitleBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.0f);
			imguiStyle.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.16f, 1.0f);
			imguiStyle.Colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.14f, 1.0f);
			imguiStyle.Colors[ImGuiCol_TabHovered] = ImVec4(0.24f, 0.24f, 0.24f, 1.0f);
			imguiStyle.Colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
			imguiStyle.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
			imguiStyle.Colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.26f, 0.26f, 1.0f);
			imguiStyle.Colors[ImGuiCol_HeaderActive] = ImVec4(0.32f, 0.32f, 0.32f, 1.0f);
			imguiStyle.Colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
			imguiStyle.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.28f, 1.0f);
			imguiStyle.Colors[ImGuiCol_ButtonActive] = ImVec4(0.36f, 0.36f, 0.36f, 1.0f);

			ImGui_ImplSDL3_InitForOpenGL(m_window, m_glContext);
			ImGui_ImplOpenGL3_Init("#version 300 es"); // matches your GL ES 3.0 context
#endif

#if !defined(__EMSCRIPTEN__) && !defined(NDEBUG)
			// Enable debug output via KHR_debug extension if supported
			if (GLAD_GL_KHR_debug && glDebugMessageCallback && glDebugMessageControl)
			{
				glEnable(GL_DEBUG_OUTPUT);

				// Forces the callback to happen on the same thread, immediately during the offending call
				glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

				// Set the callback function
				glDebugMessageCallback(MessageCallback, 0);

				GLuint ignoreIDs[] = {
					131185, // Buffer object successfully created
					131218, // Material/Shader state info
					131204	// Texture state info
				};

				// Ignore non-significant error/warning codes
				glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 3, ignoreIDs, GL_FALSE);
			}
#endif

			// Audio Stream Setup
			SDL_AudioSpec desiredSpec;
			SDL_memset(&desiredSpec, 0, sizeof(desiredSpec));
			desiredSpec.freq = audioEngine.getSampleRate();
			desiredSpec.format = SDL_AUDIO_F32;
			desiredSpec.channels = audioEngine.getChannels();

			m_audioStream =
				SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desiredSpec, nullptr, nullptr);
			if (!m_audioStream)
			{
				// Audio is not critical: log and keep running silent.
				std::cerr << "Failed to open audio stream: " << SDL_GetError() << " - continuing without audio"
						  << std::endl;
			}
			else
			{
				audioEngine.setAudioStream(m_audioStream);
				SDL_AudioDeviceID deviceID = SDL_GetAudioStreamDevice(m_audioStream);
				SDL_ResumeAudioDevice(deviceID);
			}
		}

		SDLInitializer::~SDLInitializer()
		{
#ifndef WEIRD_DISABLE_IMGUI
			ImGui_ImplOpenGL3_Shutdown();
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();
#endif

			AudioEngine::getInstance().setAudioStream(nullptr);
			if (m_audioStream)
			{
				SDL_DestroyAudioStream(m_audioStream);
				m_audioStream = nullptr;
			}

			if (m_useFBDevEGL)
			{
				ShutdownFBDevEGL();
			}
			else
			{
				SDL_GL_DestroyContext(m_glContext);
			}
			SDL_DestroyWindow(m_window);
			SDL_Quit();
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
