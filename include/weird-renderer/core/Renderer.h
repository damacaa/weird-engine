#pragma once

#include <string>
#include <vector>

#include "weird-renderer/audio/AudioEngine.h"
#include "weird-renderer/core/Display.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/core/SDF2DRenderPipeline.h"
#include "weird-renderer/core/SDF3DRenderPipeline.h"
#include "weird-renderer/core/MeshRenderPipeline.h"
#include "weird-renderer/core/SDLInitializer.h"
#include "weird-renderer/resources/DataBuffer.h"

#include "weird-engine/Scene.h"
#include "weird-renderer/core/RenderPlane.h"

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class Renderer
		{

		public:
			Renderer(const DisplaySettings& settings, SDL_Window*& window);
			~Renderer();
			void render(Scene& scene, const double time, const double delta);
			void setWindowTitle(const char* name);
			void setWindowSize(unsigned int width, unsigned int height);

			SDL_Window* getWindow();

		private:
			Texture& renderScene(Scene& scene, const double time, const double delta);

			SDL_Window* m_window;
			unsigned int m_windowWidth, m_windowHeight;
			float m_distanceSampleScale;
			float m_renderScale;
			unsigned int m_renderWidth, m_renderHeight;

			bool m_vSyncEnabled;
			float m_targetRefreshRate;
			float m_ditheringSpread;
			int m_ditheringColorCount;
			bool m_ditheringEnabled;

			// Render pipelines
			SDF2DRenderPipeline* m_worldPipeline;
			SDF2DRenderPipeline* m_uiPipeline;
			SDF3DRenderPipeline* m_3DWorldPipeline;
			MeshRenderPipeline* m_meshPipeline;

			Shader m_postProcessingShader;
			Shader m_combineScenesShaderProgram;
			Shader m_outputShaderProgram;

			RenderTarget m_combinationRender;
			RenderTarget m_outputResolutionRender;

			RenderPlane m_renderPlane;

			Texture m_combineResultTexture;
			Texture m_outputTexture;

			glm::vec4 m_colorPalette[16];
			DisplaySettings::ExtraMaterialData m_materialDataPalette[16];

			Camera m_uiCamera;

			bool m_takeScreenshot = false;
			std::string m_lastScreenshotPath;

			void output(Scene& scene, Texture& texture, const double delta);
			void freeAll();
		};

	} // namespace WeirdRenderer
} // namespace WeirdEngine
