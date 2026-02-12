#pragma once
#include <string>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class Display
		{
		public:

			inline static int width;
			inline static int height;

			inline static int rWidth;
			inline static int rHeight;

		private:
			Display();
		};

		struct DisplaySettings
		{
			int width = 800, height = 800, x = 50, y = 50;
			bool fullscreen = false, vSyncEnabled = true;
			float distanceSampleScale = 0.5f, internalResolutionScale = 1.0f;
			std::string windowTitle = "Weird Engine";
		};
	}
}

