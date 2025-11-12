#pragma once

#include <vector>

#include "RenderTarget.h"
#include "DataBuffer.h"
#include "Screen.h"
#include "SDLInitializer.h"
#include "AudioEngine.h"

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
			Renderer(const unsigned int width, const unsigned int height);
			~Renderer();
			void render(Scene& scene, const double time);
			void setWindowTitle(const char* name);

			SDL_Window* getWindow();

		private:

			AudioEngine m_audioEngine;

			SDLInitializer m_sdlInitializer;

			SDL_Window* m_window;
			unsigned int m_windowWidth, m_windowHeight;
			float m_distanceSampleScale;
			float m_renderScale;
			unsigned int m_distanceSampleWidth, m_distanceSampleHeight;
			unsigned int m_renderWidth, m_renderHeight;

			bool m_vSyncEnabled;

			Shader m_geometryShaderProgram;
			Shader m_instancedGeometryShaderProgram;
			Shader m_2DDistanceShader;
			Shader m_JumpFloodInitShader;
			Shader m_JumpFloodStepShader;
			Shader m_2DDistanceCorrectionShader;
			Shader m_2DDistanceUpscalerShader;
			Shader m_2DMaterialColorShader;
			Shader m_2DMaterialBlendShader;
			Shader m_2DGridShader;
			Shader m_2DLightingShader;
			Shader m_postProcessingShader;
			Shader m_3DsdfShaderProgram;
			Shader m_combineScenesShaderProgram;
			Shader m_outputShaderProgram;

			RenderTarget m_geometryRender;
			RenderTarget m_3DSceneRender;

			RenderTarget m_2DSceneRender;
			RenderTarget m_2DDistanceCorrectionRender;
			RenderTarget m_2DDistanceUpscaler;
			RenderTarget m_2DColorRender;
			RenderTarget m_2DPostProcessRender;
			RenderTarget m_2DBackgroundRender;

			RenderTarget m_combinationRender;
			RenderTarget m_outputResolutionRender;

			RenderPlane m_renderPlane;

			DataBuffer m_shapes2D;

			Texture m_geometryTexture;
			Texture m_geometryDepthTexture;
			Texture m_3DSceneTexture;
			Texture m_3DDepthSceneTexture;


			Texture m_distanceTexture; // World coords???
			Texture m_2DDistanceUpscaled;

			Texture m_2dColorTexture;

			// Distance correction
			Texture			m_jumpFloodInitTexture;
			RenderTarget	m_jumpFloodInitRender;

			Texture	m_JumpFloodTexturePing;
			Texture m_JumpFloodTexturePong;
			RenderTarget m_JumpFloodRenderPing;
			RenderTarget m_JumpFloodRenderPong;
			RenderTarget *m_JumpFloodDoubleBuffer[2];

			Texture m_distanceTextureCorrected; // Screen coords


			Texture	m_postProcessTextureFront;
			Texture m_postProcessTextureBack;
			RenderTarget m_postProcessRenderFront;
			RenderTarget m_postProcessRenderBack;
			RenderTarget *m_postProcessDoubleBuffer[2];

			Texture m_lit2DSceneTexture;
			Texture m_2DBackgroundTexture;

			Texture m_combineResultTexture;

			WeirdRenderer::Dot2D* m_2DData = nullptr;
			uint32_t m_2DDataSize = 0;

			uint32_t m_materialBlendIterations;


			void output(Scene& scene, Texture& texture);

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
		};


	}
}
