#pragma once
#include <string>

#include "weird-engine/vec.h"

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
			float distanceSampleScale = 0.5f, internalResolutionScale = 1.0f, refreshRate = 60.0f;
			float worldDistanceOverscan = 0.1f;
			bool enableDithering = true;
			float ditheringSpread = 0.05f;
			int ditheringColorCount = 16;
			bool enableLongShadows = false;
			vec3 shadowTint = vec3(0.4f, 0.6f, 0.8f);
			float worldSmoothFactor = 0.5f;
			float uiSmoothFactor = 3.0f;

			enum DefaultColors
			{
				Black,
				White,
				Gray,
				LightGray,
				Red,
				Green,
				Blue,
				Yellow,
				Orange,
				Purple,
				Cyan,
				Magenta,
				LightGreen,
				Pink,
				LightBlue,
				Brown
			};

			vec4 colorPalette[16] = {
				vec4(0.025f, 0.025f, 0.05f, 1.0f),	// Black
				vec4(1.0f, 1.0f, 1.0f, 1.0f),		// White
				vec4(0.484f, 0.484f, 0.584f, 1.0f), // Dark Gray
				vec4(0.752f, 0.762f, 0.78f, 1.0f),	// Light Gray
				vec4(.8f, 0.2f, 0.2f, 1.0f),		// Red
				vec4(0.1f, .95f, 0.1f, 1.0f),		// Green
				vec4(0.15f, 0.25f, .85f, 1.0f),		// Blue
				vec4(1.0f, .9f, 0.2f, 1.0f),		// Yellow
				vec4(.95f, 0.4f, 0.1f, 1.0f),		// Orange
				vec4(0.5f, 0.0f, 1.0f, 1.0f),		// Purple
				vec4(0.0f, .9f, .9f, 1.0f),			// Cyan
				vec4(1.0f, 0.3f, .6f, 1.0f),		// Magenta
				vec4(0.5f, 1.0f, 0.5f, 1.0f),		// Light Green
				vec4(1.0f, 0.5f, 0.5f, 1.0f),		// Pink
				vec4(0.5f, 0.5f, 1.0f, 1.0f),		// Light Blue
				vec4(0.4f, 0.25f, 0.1f, 1.0f)		// Brown
			};

			struct ExtraMaterialData
			{
				float metallic = 0.0f;
				float roughness = 1.0f;
			};

			ExtraMaterialData materialDataPalette[16] = {
				{0.1f, 0.3f},  // Black
				{0.1f, 0.3f},  // White
				{0.1f, 0.3f},  // Dark Gray
				{0.1f, 0.3f},  // Light Gray
				{0.1f, 0.3f},  // Red
				{0.1f, 0.3f},  // Green
				{0.1f, 0.3f},  // Blue
				{0.1f, 0.3f},  // Yellow
				{0.1f, 0.3f},  // Orange
				{0.1f, 0.3f}, // Purple
				{0.1f, 0.3f}, // Cyan
				{0.1f, 0.3f}, // Magenta
				{0.1f, 0.3f}, // Light Green
				{0.1f, 0.3f}, // Pink
				{0.1f, 0.3f}, // Light Blue
				{0.1f, 0.3f}   // Brown
			};

			std::string windowTitle = "Weird Engine";
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
