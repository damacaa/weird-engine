#pragma once

#include <SDL3/SDL.h>

#include "weird-renderer/audio/AudioEngine.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class SDLInitializer
		{
		public:
			SDLInitializer(DisplaySettings& settings, SDL_Window*& m_window, AudioEngine& audioEngine);
			~SDLInitializer();

		private:
			SDL_Window* m_window;
			SDL_GLContext m_glContext;
			SDL_AudioStream* m_audioStream;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine