#pragma once

#include <vector>

#include "RenderTarget.h"
#include "DataBuffer.h"
#include "Screen.h"
#include "SDLInitializer.h"
#include "AudioEngine.h"
#include "SDF2DRenderPipeline.h"

#include "weird-engine/Scene.h"
#include "RenderPlane.h"

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class Renderer
		{
		

		public:
			Renderer(const unsigned int width, const unsigned int height, SDL_Window*& window);
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

			// TODO: Split into color palette and materials
			// Materials can use multiple colors from the palette
			// Materials have extra parameters like "temporal permanence"
			glm::vec4 m_colorPalette[16] = {
				glm::vec4(0.025f, 0.025f, 0.05f, 1.0f), // Black
				glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), // White
				glm::vec4(0.484f, 0.484f, 0.584f, 1.0f), // Dark Gray
				glm::vec4(0.752f, 0.762f, 0.74f, 1.0f), // Light Gray
				glm::vec4(.8f, 0.2f, 0.2f, 1.0f), // Red
				glm::vec4(0.1f, .95f, 0.1f, 1.0f), // Green
				glm::vec4(0.15f, 0.25f, .85f, 1.0f), // Blue
				glm::vec4(1.0f, .9f, 0.2f, 0.33f), // Yellow
				glm::vec4(.95f, 0.4f, 0.1f, 1.0f), // Orange
				glm::vec4(0.5f, 0.0f, 1.0f, 1.0f), // Purple
				glm::vec4(0.0f, .9f, .9f, 1.0f), // Cyan
				glm::vec4(1.0f, 0.3f, .6f, 1.0f), // Magenta
				glm::vec4(0.5f, 1.0f, 0.5f, 1.0f), // Light Green
				glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), // Pink
				glm::vec4(0.5f, 0.5f, 1.0f, 1.0f), // Light Blue
				glm::vec4(0.4f, 0.25f, 0.1f, 1.0f) // Brown
			};

			void freeAll();
		};


	}
}
