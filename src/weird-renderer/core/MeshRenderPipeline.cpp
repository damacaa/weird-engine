
#include "weird-renderer/core/MeshRenderPipeline.h"

#include <imgui.h>

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		MeshRenderPipeline::MeshRenderPipeline()
		{
			m_geometryShader = Shader(SHADERS_PATH "3d/geometry.vert", SHADERS_PATH "3d/geometry.frag");
			m_instancedGeometryShader =
				Shader(SHADERS_PATH "3d/geometry_instanced.vert", SHADERS_PATH "3d/geometry.frag");
		}

		MeshRenderPipeline::~MeshRenderPipeline()
		{
			free();
		}

		Shader& MeshRenderPipeline::getShader()
		{
			return m_geometryShader;
		}

		Shader& MeshRenderPipeline::getInstancedShader()
		{
			return m_instancedGeometryShader;
		}

		Texture& MeshRenderPipeline::getDepthTexture()
		{
			return m_depthTexture;
		}

		void MeshRenderPipeline::render(Scene& scene, RenderTarget& outputTarget, const Camera& camera,
										const std::vector<Light>& lights)
		{
			m_geometryShader.use();
			m_geometryShader.setUniform("u_camMatrix", camera.cameraMatrix);
			m_geometryShader.setUniform("u_camPos", camera.position);

			if (!lights.empty())
			{
				m_geometryShader.setUniform("u_lightPos", lights[0].position);
				m_geometryShader.setUniform("u_lightDirection", lights[0].rotation);
				m_geometryShader.setUniform("u_lightColor", lights[0].color);
			}

			// TODO: render draw queue (not yet implemented)
			scene.renderModels(outputTarget, m_geometryShader, m_instancedGeometryShader);
		}

		void MeshRenderPipeline::resize(unsigned int newWidth, unsigned int newHeight)
		{
			free();

			m_depthTexture = Texture(newWidth, newHeight, Texture::TextureType::Depth);
			m_depthRender = RenderTarget(false);
			m_depthRender.bindDepthTextureToFrameBuffer(m_depthTexture);
		}

		void MeshRenderPipeline::free()
		{
			m_depthRender.free();
			m_depthTexture.dispose();
		}

		void MeshRenderPipeline::showDebugUI()
		{
			const char* label = "Mesh Render Settings";
			if (!ImGui::CollapsingHeader(label))
				return;

			ImGui::PushID(label);

			ImGui::TextDisabled("Render queue: not yet implemented");

			ImGui::PopID();
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
