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

		enum class MotionBlurMethod
		{
			AsymmetricDelta,
			FillOverride,
			Delta = AsymmetricDelta
		};

		struct DisplaySettings
		{
			int width = 800, height = 800, x = 50, y = 50;
			bool fullscreen = false, vSyncEnabled = true;
			float distanceSampleScale = 0.5f, internalResolutionScale = 1.0f, refreshRate = 60.0f;
			float worldDistanceOverscan = 0.1f;
			bool enableMaterialBlending = true;
			bool enableMotionBlur = true;
			float motionBlurBlendSpeed = 10.0f;
			MotionBlurMethod motionBlurMethod = MotionBlurMethod::AsymmetricDelta;
			bool enableDithering = true;
			float ditheringSpread = 0.05f;
			int ditheringColorCount = 16;
			bool enableSurfaceBlur = false;
			float surfaceBlurRadius = 3.0f;
			float surfaceBlurSigmaColor = 0.15f;
			bool enableLongShadows = false;
			vec3 shadowTint = vec3(0.4f, 0.6f, 0.8f);
			float worldSmoothFactor = 0.5f;
			float uiSmoothFactor = 3.0f;
			float worldAmbientOcclusionStrength = 0.2f;
			float uiAmbientOcclusionStrength = 0.5f;
			bool enable2DLigthing = true;
			float raymarching3DContrast = 1.2f;
			bool enable3DPathTracer = true;

			std::string windowTitle = "Weird Engine";
		};
	} // namespace WeirdRenderer

	using WeirdRenderer::MotionBlurMethod;
} // namespace WeirdEngine
