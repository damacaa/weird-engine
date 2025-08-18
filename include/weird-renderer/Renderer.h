#pragma once

#include <vector>

#include "RenderTarget.h"
#include "DataBuffer.h"
#include "Screen.h"

#include "weird-engine/Scene.h"
#include "RenderPlane.h"

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

inline void CheckOpenGLError(const char *file, int line)
{
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR)
	{
		// int e = err;
		std::cerr << "OpenGL Error (" << err << ") at " << file << ":" << line << std::endl;
		// Optionally, map err to a string representation
	}
}

#define GL_CHECK_ERROR() CheckOpenGLError(__FILE__, __LINE__)

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class GLInitializer {
		public:
			GLInitializer(const unsigned int width, const unsigned int height, SDL_Window*& m_window);
		};

		class Renderer
		{
		

		public:
			Renderer(const unsigned int width, const unsigned int height);
			~Renderer();
			void render(Scene& scene, const double time);
			void setWindowTitle(const char* name);

			SDL_Window* getWindow();

		private:
			GLInitializer m_initializer;

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


			Texture m_distanceTexture;
			Texture m_2dColorTexture;


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

			glm::vec3 m_colorPalette[16] = {
				vec3(0.025f, 0.025f, 0.05f), // Black
				vec3(1.0f, 1.0f, 1.0f), // White
				vec3(0.484f, 0.484f, 0.584f), // Dark Gray
				vec3(0.752f, 0.762f, 0.74f), // Light Gray
				vec3(.8f, 0.1f, 0.1f), // Red
				vec3(0.1f, .95f, 0.1f), // Green
				vec3(0.15f, 0.25f, .85f), // Blue
				vec3(1.0f, .9f, 0.2f), // Yellow
				vec3(.95f, 0.4f, 0.1f), // Orange
				vec3(0.5f, 0.0f, 1.0f), // Purple
				vec3(0.0f, .9f, .9f), // Cyan
				vec3(1.0f, 0.3f, .6f), // Magenta
				vec3(0.5f, 1.0f, 0.5f), // Light Green
				vec3(1.0f, 0.5f, 0.5f), // Pink
				vec3(0.5f, 0.5f, 1.0f), // Light Blue
				vec3(0.4f, 0.25f, 0.1f) // Brown
			};
		};


	}
}
