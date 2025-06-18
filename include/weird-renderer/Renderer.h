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
			bool checkWindowClosed() const;
			void setWindowTitle(const char* name);

			SDL_Window* getWindow();

		private:
			GLInitializer m_initializer;

			SDL_Window* m_window;
			unsigned int m_windowWidth, m_windowHeight;
			float m_renderScale;
			unsigned int m_renderWidth, m_renderHeight;

			bool m_vSyncEnabled;

			Shader m_geometryShaderProgram;
			Shader m_instancedGeometryShaderProgram;
			Shader m_2DsdfShaderProgram;
			Shader m_postProcessShaderProgram;
			Shader m_3DsdfShaderProgram;
			Shader m_combineScenesShaderProgram;
			Shader m_outputShaderProgram;

			RenderTarget m_geometryRender;
			RenderTarget m_3DSceneRender;

			RenderTarget m_2DSceneRender;
			RenderTarget m_2DPostProcessRender;

			RenderTarget m_combinationRender;
			RenderTarget m_outputResolutionRender;

			RenderPlane m_renderPlane;

			DataBuffer m_shapes2D;

			Texture m_geometryTexture;
			Texture m_geometryDepthTexture;
			Texture m_3DSceneTexture;
						Texture m_3DDepthSceneTexture;


			Texture m_distanceTexture;
			Texture m_lit2DSceneTexture;

			Texture m_combineResultTexture;

			WeirdRenderer::Dot2D* m_2DData = nullptr;
			uint32_t m_2DDataSize = 0;

			void renderFire(Scene& scene, Camera& camera, float time);
			void renderGeometry(Scene& scene, Camera& camera);
			void output(Scene& scene, Texture& texture);
		};


	}
}
