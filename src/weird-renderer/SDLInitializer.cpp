#include "weird-renderer/SDLInitializer.h"
#include "weird-renderer/AudioEngine.h"
#include <iostream>
#include <stdexcept>
#include <glad/glad.h>

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

		SDLInitializer::SDLInitializer(const unsigned int width, const unsigned int height, SDL_Window*& window, AudioEngine& audioEngine) : m_window(window)
        {
            audioEngine.init();

            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
            {
                throw std::runtime_error("SDL could not initialize!");
            }

            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

            window = SDL_CreateWindow("Weird Engine", width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
            if (!window) {
                throw std::runtime_error("Failed to create SDL window.");
            }

            m_window = window;

            m_glContext = SDL_GL_CreateContext(m_window);
            if (!m_glContext) {
                throw std::runtime_error("Failed to create OpenGL context.");
            }

            SDL_GL_MakeCurrent(m_window, m_glContext);

            if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
                throw std::runtime_error("Failed to initialize GLAD.");
            }

#ifndef NDEBUG
			// Enable debug output
			glEnable(GL_DEBUG_OUTPUT);

			// Forces the callback to happen on the same thread, immediately during the offending call
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

			// Set the callback function
			glDebugMessageCallback(MessageCallback, 0);

			GLuint ignoreIDs[] = {
                131185, // Buffer object successfully created
                131218, // Material/Shader state info
                131204 // Texture state info
            };

            // Ignore non-significant error/warning codes
			glDebugMessageControl(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 3, ignoreIDs, GL_FALSE);
#endif

			// Audio Stream Setup
            SDL_AudioSpec desiredSpec;
            SDL_memset(&desiredSpec, 0, sizeof(desiredSpec));
            desiredSpec.freq = audioEngine.getSampleRate();
            desiredSpec.format = SDL_AUDIO_F32;
            desiredSpec.channels = audioEngine.getChannels();

            m_audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desiredSpec, AudioEngine::data_callback, &audioEngine);
            if (!m_audioStream) {
                throw std::runtime_error("Failed to open audio stream.");
            }

            SDL_AudioDeviceID deviceID = SDL_GetAudioStreamDevice(m_audioStream);
            SDL_ResumeAudioDevice(deviceID);
        }

        SDLInitializer::~SDLInitializer() {
            SDL_DestroyAudioStream(m_audioStream);
            SDL_GL_DestroyContext(m_glContext);
            SDL_DestroyWindow(m_window);
            SDL_Quit();
        }
    }
}
