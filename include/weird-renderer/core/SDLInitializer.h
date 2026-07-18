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

			// True when rendering through the direct framebuffer EGL backend
			// (WEIRD_VIDEO_BACKEND=fbdev) instead of SDL's GL context.
			bool m_useFBDevEGL = false;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine