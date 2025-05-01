#pragma once

#include <vector>

#include "RenderPlane.h"
#include "DataBuffer.h"

#include "../weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class Renderer
		{
		private:
			GLFWwindow* m_window;
			unsigned int m_windowWidth, m_windowHeight;
			double m_renderScale;
			unsigned int m_renderWidth, m_renderHeight;

			bool m_vSyncEnabled;
			bool m_renderMeshesOnly;

			Shader m_geometryShaderProgram;
			Shader m_instancedGeometryShaderProgram;
			Shader m_2DsdfShaderProgram;
			Shader m_postProcessShaderProgram;
			Shader m_3DsdfShaderProgram;
			Shader m_combineScenesShaderProgram;
			Shader m_outputShaderProgram;

			RenderPlane m_geometryRenderPlane;
			RenderPlane m_3DRenderPlane;

			RenderPlane m_sdfRenderPlane;
			RenderPlane m_postProcessRenderPlane;

			RenderPlane m_combinationRenderPlane;
			RenderPlane m_outputRenderPlane;

			DataBuffer m_shapes2D;

			Texture m_geometryTexture;
			Texture m_geometryDepthTexture;
			Texture m_3DSceneTexture;

			Texture m_distanceTexture;
			Texture m_lit2DSceneTexture;

			Texture m_combineResultTexture;

			void renderGeometry(Scene& scene, Camera& camera);

		public:
			Renderer(const unsigned int width, const unsigned int height);
			~Renderer();
			void render(Scene& scene, const double time);
			bool checkWindowClosed() const;
			void setWindowTitle(const char* name);

			GLFWwindow* getWindow();
		};
	}
}
