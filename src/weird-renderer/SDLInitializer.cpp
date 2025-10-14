#include "weird-renderer/SDLInitializer.h"
#include "weird-renderer/AudioEngine.h"
#include <iostream>
#include <stdexcept>
#include <glad/glad.h>

namespace WeirdEngine {
    namespace WeirdRenderer {
        SDLInitializer::SDLInitializer(const unsigned int width, const unsigned int height, SDL_Window*& window, AudioEngine& audioEngine) : m_window(window) {
            if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
                throw std::runtime_error("SDL could not initialize!");
            }

            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
            SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
            SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

            m_window = SDL_CreateWindow("Weird Engine", width, height, SDL_WINDOW_OPENGL);
            if (!m_window) {
                throw std::runtime_error("Failed to create SDL window.");
            }

            m_glContext = SDL_GL_CreateContext(m_window);
            if (!m_glContext) {
                throw std::runtime_error("Failed to create OpenGL context.");
            }

            SDL_GL_MakeCurrent(m_window, m_glContext);

            if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
                throw std::runtime_error("Failed to initialize GLAD.");
            }

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
