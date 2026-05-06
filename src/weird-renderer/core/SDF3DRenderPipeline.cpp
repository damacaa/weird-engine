
#include "weird-renderer/core/SDF3DRenderPipeline.h"

#include <algorithm>
#include <imgui.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		SDF3DRenderPipeline::SDF3DRenderPipeline(const Config& config, const glm::vec4* colorPalette, const DisplaySettings::ExtraMaterialData* materialDataPalette, RenderPlane& renderPlane)
			: m_config(config)
			, m_colorPalette(colorPalette)
			, m_materialDataPalette(materialDataPalette)
			, m_renderPlane(renderPlane)
			, m_shapeDataBuffer(nullptr)
			, m_accumIdx(0)
			, m_frameCounter(0)
			, m_oldCameraMatrix(glm::mat4(0.0f))
		{
			m_sdfShader = Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "3d/sdf_raymarching.frag");
			m_shapeDataBuffer = new DataBuffer();
			resize(config.renderWidth, config.renderHeight);

			if (m_config.enablePathTracer)
			{
				m_sdfShader.addDefine("PATH_TRACING");
			}
		}

		SDF3DRenderPipeline::~SDF3DRenderPipeline()
		{
			free();
			delete m_shapeDataBuffer;
		}

		Shader& SDF3DRenderPipeline::getShader()
		{
			return m_sdfShader;
		}

		void SDF3DRenderPipeline::render(
			vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
			const std::vector<Light>& lights,
			const Camera& camera,
			double time,
			Texture& geometryDepthTexture
		)
		{
			// Reset frame counter when path tracer is disabled (no accumulation)
			if (!m_config.enablePathTracer)
				m_frameCounter = 0;

			// Detect camera movement and restart accumulation
			bool cameraMoved = (m_oldCameraMatrix != camera.view);
			if (cameraMoved || m_sdfShader.hasRecompiled())
			{
				m_frameCounter = 0;
				m_oldCameraMatrix = camera.view;
			}

			int previousAccumIdx = m_accumIdx;
			m_accumIdx = (m_accumIdx + 1) % 2;

			m_accumRender[m_accumIdx].bind();

			m_sdfShader.use();

			float shaderFov = 1.0f / tan(camera.fov * 0.5f * 0.01745329f); // PI / 180
			m_sdfShader.setUniform("u_camMatrix", camera.view);
			m_sdfShader.setUniform("u_fov", shaderFov);
			m_sdfShader.setUniform("u_time", (float)time);
			m_sdfShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));
			m_sdfShader.setUniform("u_staticColors", m_colorPalette, 16);

			for (int i = 0; i < 16; i++)
			{
				std::string prefix = "u_materialData[" + std::to_string(i) + "].";
				m_sdfShader.setUniform(prefix + "metallic", m_materialDataPalette[i].metallic);
				m_sdfShader.setUniform(prefix + "roughness", m_materialDataPalette[i].roughness);
			}
			m_sdfShader.setUniform("u_near", NEAR_PLANE);
			m_sdfShader.setUniform("u_far", FAR_PLANE);
			m_sdfShader.setUniform("u_frameCounter", m_frameCounter);
			m_sdfShader.setUniform("u_rayBounces", m_config.rayBounces);

			int numLights = std::min((int)lights.size(), 8);
			m_sdfShader.setUniform("u_numLights", numLights);
			for (int i = 0; i < numLights; i++)
			{
				std::string prefix = "u_lights[" + std::to_string(i) + "].";
				m_sdfShader.setUniform(prefix + "position", lights[i].position);
				m_sdfShader.setUniform(prefix + "direction", lights[i].rotation);
				m_sdfShader.setUniform(prefix + "color", lights[i].color);
				m_sdfShader.setUniform(prefix + "type", (int)lights[i].type);
			}

			// Previous accumulation frame and geometry depth
			m_sdfShader.setUniform("t_previousColor", 0);
			m_accumTexture[previousAccumIdx].bind(0);

			m_sdfShader.setUniform("t_depthTexture", 1);
			geometryDepthTexture.bind(1);

			// Shape data buffer
			m_sdfShader.setUniform("t_shapeBuffer", 2);
			m_shapeDataBuffer->uploadData<vec4>(shapeData, dataSize);
			m_shapeDataBuffer->bind(2);

			m_sdfShader.setUniform("u_loadedObjects", (int)dataSize);
			m_sdfShader.setUniform("u_customShapeCount", (int)shapeCount);

			if (m_frameCounter < m_config.maxAccumulationFrames)
				m_renderPlane.draw(m_sdfShader);

			m_frameCounter++;

			// Blit accumulation result to the main output target
			m_outputRender.bind();
			glBindFramebuffer(GL_READ_FRAMEBUFFER, m_accumRender[m_accumIdx].getFrameBuffer());
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_outputRender.getFrameBuffer());
			glBlitFramebuffer(
				0, 0, (GLint)m_config.renderWidth, (GLint)m_config.renderHeight,
				0, 0, (GLint)m_config.renderWidth, (GLint)m_config.renderHeight,
				GL_COLOR_BUFFER_BIT, GL_NEAREST);
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

			// Keep output bound so geometry can be rendered on top
			m_outputRender.bind();

			glFinish();
		}

		RenderTarget& SDF3DRenderPipeline::getRenderTarget()
		{
			return m_outputRender;
		}

		Texture& SDF3DRenderPipeline::getOutputTexture()
		{
			return m_outputTexture;
		}

		void SDF3DRenderPipeline::resize(unsigned int newWidth, unsigned int newHeight)
		{
			free();

			m_config.renderWidth = newWidth;
			m_config.renderHeight = newHeight;

			m_outputTexture = Texture(newWidth, newHeight, Texture::TextureType::Data);
			m_depthTexture = Texture(newWidth, newHeight, Texture::TextureType::Depth);
			m_outputRender = RenderTarget(false);
			m_outputRender.bindColorTextureToFrameBuffer(m_outputTexture);
			m_outputRender.bindDepthTextureToFrameBuffer(m_depthTexture);

			m_accumTexture[0] = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_accumTexture[1] = Texture(newWidth, newHeight, Texture::TextureType::LinearData);
			m_accumRender[0] = RenderTarget(false);
			m_accumRender[0].bindColorTextureToFrameBuffer(m_accumTexture[0]);
			m_accumRender[0].bindDepthTextureToFrameBuffer(m_depthTexture);
			m_accumRender[1] = RenderTarget(false);
			m_accumRender[1].bindColorTextureToFrameBuffer(m_accumTexture[1]);
			m_accumRender[1].bindDepthTextureToFrameBuffer(m_depthTexture);

			m_accumIdx = 0;
			m_frameCounter = 0;
		}

		void SDF3DRenderPipeline::free()
		{
			m_outputRender.free();
			m_accumRender[0].free();
			m_accumRender[1].free();

			m_outputTexture.dispose();
			m_depthTexture.dispose();
			m_accumTexture[0].dispose();
			m_accumTexture[1].dispose();
		}

		void SDF3DRenderPipeline::showDebugUI()
		{
			const char* label = "3D SDF Settings";
			if (!ImGui::CollapsingHeader(label))
				return;

			ImGui::PushID(label);

			if (ImGui::Checkbox("Enable Path Tracer", &m_config.enablePathTracer))
			{	
				std::cout << "Path tracer " << (m_config.enablePathTracer ? "enabled" : "disabled") << std::endl;

				if(m_config.enablePathTracer)
					m_sdfShader.addDefine("PATH_TRACING");
				else
					m_sdfShader.removeDefine("PATH_TRACING");
			}


			if (m_config.enablePathTracer)
			{
				if(ImGui::SliderInt("Bounces", &m_config.rayBounces, 1, 10))
					m_frameCounter = 0;

				if (ImGui::SliderInt("Max Accumulation Frames", &m_config.maxAccumulationFrames, 10, 1000))
					m_frameCounter = 0;

				int progress = std::min(m_frameCounter, m_config.maxAccumulationFrames);
				ImGui::LabelText("Accumulation Progress", "%d / %d frames", progress, m_config.maxAccumulationFrames);
			}

			ImGui::PopID();
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
