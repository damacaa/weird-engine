
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

		Texture& MeshRenderPipeline::getGBufferMaterial()
		{
			return m_gbufferMaterial;
		}

		Texture& MeshRenderPipeline::getDepthTexture()
		{
			return m_depthTexture;
		}

		Texture& MeshRenderPipeline::getBackDepthTexture()
		{
			return m_backDepthTexture;
		}

		void MeshRenderPipeline::render(Scene& scene, RenderTarget& outputTarget, const Camera& camera,
									const std::vector<Light>& lights)
		{
			// Set GBuffer uniforms for both shaders
			m_gbufferShader.use();
			m_gbufferShader.setUniform("u_camMatrix", camera.cameraMatrix);

			m_gbufferInstancedShader.use();
			m_gbufferInstancedShader.setUniform("u_camMatrix", camera.cameraMatrix);

			// Bind GBuffer FBO and activate all 4 colour draw buffers
			m_gbufferRender.bind();
			const GLenum drawBuffers[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};
			glDrawBuffers(4, drawBuffers);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// Render scene meshes into the GBuffer.
			const auto& drawQueue = scene.getDrawQueue();
			for (const auto& cmd : drawQueue)
			{
				cmd.mesh->draw(m_gbufferShader, camera, cmd.translation, cmd.rotation, cmd.scale, cmd.materialIndex);
			}

			// Restore single draw buffer so subsequent passes don't accidentally write to all 3
			const GLenum single[1] = {GL_COLOR_ATTACHMENT0};
			glDrawBuffers(1, single);

			// --- Back-face depth pass for slab shadow technique ---
			{
				m_backDepthRender.bind();

				const GLenum noneArray[1] = {GL_NONE};
				glDrawBuffers(1, noneArray);
				glReadBuffer(GL_NONE);

				glClearDepthf(0.0f);
				glClear(GL_DEPTH_BUFFER_BIT);

				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_GREATER);
				glDepthMask(GL_TRUE);
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);

				m_gbufferShader.use();

				for (const auto& cmd : drawQueue)
				{
					cmd.mesh->draw(m_gbufferShader, camera, cmd.translation, cmd.rotation, cmd.scale, cmd.materialIndex);
				}

				glCullFace(GL_BACK);
				glDepthFunc(GL_LEQUAL);
				glClearDepthf(1.0f);

				glDrawBuffers(1, single);
			}
		}

		void MeshRenderPipeline::resize(unsigned int newWidth, unsigned int newHeight)
		{
			free();

			m_gbufferAlbedo   = Texture(newWidth, newHeight, Texture::TextureType::Data);
			m_gbufferWorldPos = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_gbufferNormal   = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_gbufferMaterial = Texture(newWidth, newHeight, Texture::TextureType::IntData);
			m_depthTexture    = Texture(newWidth, newHeight, Texture::TextureType::Depth);

			m_gbufferRender = RenderTarget(false);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferAlbedo,   0);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferWorldPos, 1);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferNormal,   2);
			m_gbufferRender.bindColorTextureToFrameBuffer(m_gbufferMaterial, 3);
			m_gbufferRender.bindDepthTextureToFrameBuffer(m_depthTexture);

			m_backDepthTexture = Texture(newWidth, newHeight, Texture::TextureType::Depth);
			m_backDepthRender  = RenderTarget(false);
			m_backDepthRender.bindDepthTextureToFrameBuffer(m_backDepthTexture);
		}

		void MeshRenderPipeline::free()
		{
			m_gbufferRender.free();
			m_backDepthRender.free();
			m_gbufferAlbedo.dispose();
			m_gbufferWorldPos.dispose();
			m_gbufferNormal.dispose();
			m_gbufferMaterial.dispose();
			m_depthTexture.dispose();
			m_backDepthTexture.dispose();
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
