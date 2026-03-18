#pragma once

#include <vector>

#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DataBuffer.h"
#include "weird-renderer/core/Display.h"
#include "weird-renderer/core/SDLInitializer.h"
#include "weird-renderer/audio/AudioEngine.h"
#include "weird-renderer/core/SDF2DRenderPipeline.h"

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

			SDL_Window* m_window;
			unsigned int m_windowWidth, m_windowHeight;
			float m_distanceSampleScale;
			float m_renderScale;
			unsigned int m_renderWidth, m_renderHeight;

			bool m_vSyncEnabled;
			float m_targetRefreshRate;

			// Pipelines for 2D SDF rendering
			SDF2DRenderPipeline* m_worldPipeline;
			SDF2DRenderPipeline* m_uiPipeline;

			Shader m_geometryShaderProgram;
			Shader m_instancedGeometryShaderProgram;
			Shader m_postProcessingShader;
			Shader m_3DsdfShaderProgram;
			Shader m_combineScenesShaderProgram;
			Shader m_outputShaderProgram;

			RenderTarget m_geometryRender;
			RenderTarget m_3DSceneRender;

			RenderTarget m_combinationRender;
			RenderTarget m_outputResolutionRender;

			RenderPlane m_renderPlane;

			Texture m_geometryTexture;
			Texture m_geometryDepthTexture;
			Texture m_3DSceneTexture;
			Texture m_3DDepthSceneTexture;
			DataBuffer* m_3DShapeDataBuffer;

			Texture m_combineResultTexture;

			void output(Scene& scene, Texture& texture, const double delta);

			glm::vec4 m_colorPalette[16];

			Camera m_uiCamera;

			void freeAll();
		};


	}
}
