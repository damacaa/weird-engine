
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

			m_gbufferShader = Shader(SHADERS_PATH "3d/geometry.vert", SHADERS_PATH "3d/gbuffer.frag");
			m_gbufferInstancedShader =
				Shader(SHADERS_PATH "3d/geometry_instanced.vert", SHADERS_PATH "3d/gbuffer.frag");
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

Texture& MeshRenderPipeline::getGBufferAlbedo()
		{
			return m_gbufferAlbedo;
		}

		Texture& MeshRenderPipeline::getGBufferWorldPos()
		{
			return m_gbufferWorldPos;
		}

		Texture& MeshRenderPipeline::getGBufferNormal()
		{
			return m_gbufferNormal;
		}

		Texture& MeshRenderPipeline::getDepthTexture()
		{
			return m_depthTexture;
		}

		void MeshRenderPipeline::render(Scene& scene, RenderTarget& outputTarget, const Camera& camera,
									const std::vector<Light>& lights)
		{
			// Set GBuffer uniforms for both shaders
			m_gbufferShader.use();
			m_gbufferShader.setUniform("u_camMatrix", camera.cameraMatrix);

			m_gbufferInstancedShader.use();
			m_gbufferInstancedShader.setUniform("u_camMatrix", camera.cameraMatrix);

			// Bind GBuffer FBO and activate all 3 colour draw buffers
			m_gbufferRender.bind();
			const GLenum drawBuffers[3] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
			glDrawBuffers(3, drawBuffers);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Render scene meshes into the GBuffer.
			// outputTarget is forwarded to Scene::onRender for per-scene custom rendering.
			scene.renderModels(outputTarget, m_gbufferShader, m_gbufferInstancedShader);

			// Restore single draw buffer so subsequent passes don't accidentally write to all 3
			const GLenum single[1] = {GL_COLOR_ATTACHMENT0};
			glDrawBuffers(1, single);
		}

		void MeshRenderPipeline::resize(unsigned int newWidth, unsigned int newHeight)
		{
			free();

			m_gbufferAlbedo   = Texture(newWidth, newHeight, Texture::TextureType::Data);
			m_gbufferWorldPos = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_gbufferNormal   = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_depthTexture    = Texture(newWidth, newHeight, Texture::TextureType::Depth);

			m_gbufferRender = RenderTarget(false);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferAlbedo,   0);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferWorldPos, 1);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferNormal,   2);
			m_gbufferRender.bindDepthTextureToFrameBuffer(m_depthTexture);
		}

		void MeshRenderPipeline::free()
		{
			m_gbufferRender.free();
			m_gbufferAlbedo.dispose();
			m_gbufferWorldPos.dispose();
			m_gbufferNormal.dispose();
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
