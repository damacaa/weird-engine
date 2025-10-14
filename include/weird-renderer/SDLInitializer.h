#pragma once

#include <SDL3/SDL.h>
#include "AudioEngine.h"

namespace WeirdEngine {
    namespace WeirdRenderer {
        class SDLInitializer {
        public:
            SDLInitializer(const unsigned int width, const unsigned int height, SDL_Window*& m_window, AudioEngine& audioEngine);
            ~SDLInitializer();
        private:
            SDL_Window* m_window;
            SDL_GLContext m_glContext;
            SDL_AudioStream* m_audioStream;
        };
    }
}