#include "weird-renderer/SDF2DRenderPipeline.h"
#include "weird-engine/vec.h"
#include <algorithm>

namespace WeirdEngine {
	namespace WeirdRenderer {

		static int largestPowerOfTwoBelow(int n) {
			int p = 1;
			while (p * 2 <= n) {
				p *= 2;
			}
			return p;
		}

		int SDF2DRenderPipeline::largestPowerOfTwoBelow(int n) {
			return ::WeirdEngine::WeirdRenderer::largestPowerOfTwoBelow(n);
		}

		SDF2DRenderPipeline::SDF2DRenderPipeline(const Config& config, const glm::vec4* colorPalette, RenderPlane& renderPlane)
			: m_config(config)
			, m_colorPalette(colorPalette)
			, m_renderPlane(renderPlane)
			, m_distanceSampleWidth(config.renderWidth * config.distanceSampleScale)
			, m_distanceSampleHeight(config.renderHeight * config.distanceSampleScale)
			, m_materialBlendIterations(config.materialBlendIterations)
			, m_oldCameraMatrix(1.0f)
			, m_lastCameraPosition(0.0f)
		{
			// Load shaders
			m_distanceShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DSDFDistanceShader.frag");
			m_distanceShader.addDefine("BLEND_SHAPES");
			if (m_config.enableMotionBlur) {
				m_distanceShader.addDefine("MOTION_BLUR");
			}
			if (m_config.originAtBottomLeft) {
				m_distanceShader.addDefine("ORIGIN_AT_BOTTOM_LEFT");
			}

			m_jumpFloodInitShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "InitJumpFlooding.frag");
			m_jumpFloodStepShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "JumpFlooding.frag");
			m_distanceCorrectionShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DDistanceCorrection.frag");
			m_distanceUpscalerShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DDistanceUpscaler.frag");
			m_materialColorShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialColorShader.frag");
			m_materialBlendShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialBlendShader.frag");
			m_defaultBackgroundShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DBackground.frag");
			
			m_lightingShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DLightingShader.frag");
			if (m_config.enableShadows) {
				m_lightingShader.addDefine("SHADOWS_ENABLED");
			}
			if (m_config.enableRefraction) {
				m_lightingShader.addDefine("REFRACTION");
			}
			if (m_config.enableDithering) {
				m_lightingShader.addDefine("DITHERING");
			}
			if (m_config.enableAntialiasing) {
				m_lightingShader.addDefine("ANTIALIASING");
			}

			if(m_config.debugDistanceField) {
				m_lightingShader.addDefine("DEBUG_SHOW_DISTANCE");
			}
			if(m_config.debugMaterialColors) {
				m_lightingShader.addDefine("DEBUG_SHOW_COLORS");
			}

			// Initialize textures and render targets
			m_distanceTextureA = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderA = RenderTarget(false);
			m_distanceRenderA.bindColorTextureToFrameBuffer(m_distanceTextureA);

			m_distanceTextureB = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderB = RenderTarget(false);
			m_distanceRenderB.bindColorTextureToFrameBuffer(m_distanceTextureB);

			m_distanceTextureDoubleBuffer[0] = &m_distanceRenderA;
			m_distanceTextureDoubleBuffer[1] = &m_distanceRenderB;
			m_distanceTextureDoubleBufferIdx = 0;

			m_jumpFloodInitTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_jumpFloodInitRender = RenderTarget(false);
			m_jumpFloodInitRender.bindColorTextureToFrameBuffer(m_jumpFloodInitTexture);

			m_jumpFloodTexturePing = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodTexturePong = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodRenderPing = RenderTarget(false);
			m_jumpFloodRenderPing.bindColorTextureToFrameBuffer(m_jumpFloodTexturePing);
			m_jumpFloodRenderPong = RenderTarget(false);
			m_jumpFloodRenderPong.bindColorTextureToFrameBuffer(m_jumpFloodTexturePong);
			m_jumpFloodDoubleBuffer[0] = &m_jumpFloodRenderPing;
			m_jumpFloodDoubleBuffer[1] = &m_jumpFloodRenderPong;

			m_distanceTextureCorrected = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_distanceCorrectionRender = RenderTarget(false);
			m_distanceCorrectionRender.bindColorTextureToFrameBuffer(m_distanceTextureCorrected);

			m_distanceUpscaled = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_distanceUpscaler = RenderTarget(false);
			m_distanceUpscaler.bindColorTextureToFrameBuffer(m_distanceUpscaled);

			m_colorTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_colorRender = RenderTarget(false);
			m_colorRender.bindColorTextureToFrameBuffer(m_colorTexture);

			m_postProcessTextureFront = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderFront = RenderTarget(false);
			m_postProcessRenderFront.bindColorTextureToFrameBuffer(m_postProcessTextureFront);

			m_postProcessTextureBack = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderBack = RenderTarget(false);
			m_postProcessRenderBack.bindColorTextureToFrameBuffer(m_postProcessTextureBack);

			m_postProcessDoubleBuffer[0] = &m_postProcessRenderFront;
			m_postProcessDoubleBuffer[1] = &m_postProcessRenderBack;

			m_backgroundTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundRender = RenderTarget(false);
			m_backgroundRender.bindColorTextureToFrameBuffer(m_backgroundTexture);

			m_litSceneTexture = Texture(m_config.renderWidth, m_config.renderHeight, 
				m_config.renderScale <= 0.5f ? Texture::TextureType::RetroColor : Texture::TextureType::Data);
			m_litSceneRender = RenderTarget(false);
			m_litSceneRender.bindColorTextureToFrameBuffer(m_litSceneTexture);
		}

		SDF2DRenderPipeline::~SDF2DRenderPipeline()
		{
			free();
		}

		void SDF2DRenderPipeline::free()
		{
			m_distanceShader.free();
			m_jumpFloodInitShader.free();
			m_jumpFloodStepShader.free();
			m_distanceCorrectionShader.free();
			m_distanceUpscalerShader.free();
			m_materialColorShader.free();
			m_materialBlendShader.free();
			m_defaultBackgroundShader.free();
			m_lightingShader.free();

			m_distanceRenderA.free();
			m_distanceRenderB.free();
			m_jumpFloodInitRender.free();
			m_jumpFloodRenderPing.free();
			m_jumpFloodRenderPong.free();
			m_distanceCorrectionRender.free();
			m_distanceUpscaler.free();
			m_colorRender.free();
			m_postProcessRenderFront.free();
			m_postProcessRenderBack.free();
			m_backgroundRender.free();
			m_litSceneRender.free();

			m_distanceTextureA.dispose();
			m_distanceTextureB.dispose();
			m_jumpFloodInitTexture.dispose();
			m_jumpFloodTexturePing.dispose();
			m_jumpFloodTexturePong.dispose();
			m_distanceTextureCorrected.dispose();
			m_distanceUpscaled.dispose();
			m_colorTexture.dispose();
			m_postProcessTextureFront.dispose();
			m_postProcessTextureBack.dispose();
			m_backgroundTexture.dispose();
			m_litSceneTexture.dispose();
		}

		Texture& SDF2DRenderPipeline::render(WeirdRenderer::Dot2D* shapeData, uint32_t dataSize, const Camera& camera, double time, double delta, Texture* backgroundTexture)
		{
			// Execute all pipeline stages
			renderDistanceField(shapeData, dataSize, camera, time, delta);
			
			if (m_config.useCorrectedDistance) {
				applyJumpFloodCorrection(time);
			}
			
			upscaleDistance();
			renderMaterialColors(camera, time, delta);
			blendMaterials(time);
			renderBackground(camera, time);
			applyLighting(camera, time, backgroundTexture);

			return m_litSceneTexture;
		}

		void SDF2DRenderPipeline::renderDistanceField(WeirdRenderer::Dot2D* shapeData, uint32_t dataSize, const Camera& camera, double time, double delta)
		{
			int previousDistanceIndex = m_distanceTextureDoubleBufferIdx;
			m_distanceTextureDoubleBufferIdx = (m_distanceTextureDoubleBufferIdx + 1) % 2;

			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->bind();
			m_distanceShader.use();

			// Set uniforms
			m_distanceShader.setUniform("u_camMatrix", camera.view);
			m_distanceShader.setUniform("u_oldCamMatrix", m_oldCameraMatrix);
			m_oldCameraMatrix = camera.view;

			
			cameraPositionChange = camera.position - m_lastCameraPosition;
			m_lastCameraPosition = camera.position;
			m_distanceShader.setUniform("u_camPositionChange", cameraPositionChange);

			m_distanceShader.setUniform("u_time", time);
			m_distanceShader.setUniform("u_deltaTime", static_cast<float>(delta));
			m_distanceShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
			m_distanceShader.setUniform("u_blendIterations", 1);

			m_distanceShader.setUniform("t_colorTexture", 0);
			m_distanceTextureDoubleBuffer[previousDistanceIndex]->getColorAttachment()->bind(0);

			m_distanceShader.setUniform("u_loadedObjects", (int)dataSize);
			m_distanceShader.setUniform("t_shapeBuffer", 1);
			m_shapeDataBuffer.uploadData<Dot2D>(shapeData, dataSize);
			m_shapeDataBuffer.bind(1);

			m_renderPlane.draw(m_distanceShader);

			m_shapeDataBuffer.unbind();
		}

		void SDF2DRenderPipeline::applyJumpFloodCorrection(double time)
		{
			float maxDim = std::max<float>(m_distanceSampleWidth, m_distanceSampleHeight);
			uint16_t jumpFloodIterations = largestPowerOfTwoBelow(maxDim);
			bool pingpong = true;

			// Initialize
			m_jumpFloodInitRender.bind();
			m_jumpFloodInitShader.use();

			m_distanceCorrectionShader.setUniform("t_distanceTexture", 0);
			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_renderPlane.draw(m_jumpFloodInitShader);

			// Jump flood passes
			m_jumpFloodStepShader.use();
			m_jumpFloodStepShader.setUniform("t_prevSeeds", 0);
			m_jumpFloodStepShader.setUniform("u_texelSize", glm::vec2(1.0f / m_distanceSampleWidth, 1.0f / m_distanceSampleHeight));

			float jump = jumpFloodIterations;
			bool first = true;

			while (jump >= 1)
			{
				m_jumpFloodDoubleBuffer[pingpong]->bind();

				vec2 uJumpSize;
				uJumpSize.x = float(jump) / float(m_distanceSampleWidth);
				uJumpSize.y = float(jump) / float(m_distanceSampleHeight);
				m_jumpFloodStepShader.setUniform("u_jumpSize", uJumpSize);

				if (first)
				{
					m_jumpFloodInitTexture.bind(0);
					first = false;
				}
				else
				{
					m_jumpFloodDoubleBuffer[!pingpong]->getColorAttachment()->bind(0);
				}

				m_renderPlane.draw(m_jumpFloodStepShader);

				pingpong = !pingpong;
				jump /= 2;
			}

			// Distance correction
			m_distanceCorrectionRender.bind();
			m_distanceCorrectionShader.use();
			m_distanceCorrectionShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
			m_distanceCorrectionShader.setUniform("u_time", time);

			m_distanceCorrectionShader.setUniform("t_originalDistanceTexture", 0);
			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_distanceCorrectionShader.setUniform("t_distanceTexture", 1);
			int lastIndex = pingpong ? 0 : 1;
			m_jumpFloodDoubleBuffer[lastIndex]->getColorAttachment()->bind(1);

			m_renderPlane.draw(m_distanceCorrectionShader);
		}

		void SDF2DRenderPipeline::upscaleDistance()
		{
			m_distanceUpscaler.bind();

			m_distanceUpscalerShader.use();
			m_distanceUpscalerShader.setUniform("u_originalResolution", vec2(m_distanceSampleWidth, m_distanceSampleHeight));
			m_distanceUpscalerShader.setUniform("u_targetResolution", vec2(m_config.renderWidth, m_config.renderHeight));
			m_distanceUpscalerShader.setUniform("t_data", 0);

			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_renderPlane.draw(m_distanceUpscalerShader);
		}

		void SDF2DRenderPipeline::renderMaterialColors(const Camera& camera, double time, double delta)
		{
			m_colorRender.bind();

			m_materialColorShader.use();
			m_materialColorShader.setUniform("u_camMatrix", camera.view);
			m_materialColorShader.setUniform("u_time", time);
			m_materialColorShader.setUniform("u_deltaTime", delta);
			m_materialColorShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));
			m_materialColorShader.setUniform("u_staticColors", m_colorPalette, 16);
			m_materialColorShader.setUniform("u_materialBlendSpeed", m_config.materialBlendSpeed);
			m_materialColorShader.setUniform("u_camPositionChange", cameraPositionChange);


			m_materialColorShader.setUniform("t_materialDataTexture", 0);
			m_distanceUpscaled.bind(0);

			m_materialColorShader.setUniform("t_currentColorTexture", 1);
			m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(1);

			m_renderPlane.draw(m_materialColorShader);
		}

		void SDF2DRenderPipeline::blendMaterials(double time)
		{
			m_materialBlendShader.use();
			m_materialBlendShader.setUniform("t_colorTexture", 0);
			m_materialBlendShader.setUniform("u_time", time);

			for (unsigned int i = 0; i < m_materialBlendIterations * 2; i++)
			{
				m_postProcessDoubleBuffer[horizontal]->bind();

				m_materialBlendShader.setUniform("u_horizontal", horizontal);
				if (i == 0)
				{
					m_colorTexture.bind(0);
				}
				else
				{
					m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
				}

				m_renderPlane.draw(m_materialBlendShader);

				horizontal = !horizontal;
			}
		}

		void SDF2DRenderPipeline::renderBackground(const Camera& camera, double time)
		{
			m_backgroundRender.bind();

			m_defaultBackgroundShader.use();
			m_defaultBackgroundShader.setUniform("u_camMatrix", camera.view);
			m_defaultBackgroundShader.setUniform("u_time", time);
			m_defaultBackgroundShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));

			m_renderPlane.draw(m_defaultBackgroundShader);
		}

		void SDF2DRenderPipeline::applyLighting(const Camera& camera, double time, Texture* backgroundTexture)
		{
			m_litSceneRender.bind();

			m_lightingShader.use();
			m_lightingShader.setUniform("u_camMatrix", camera.view);
			m_lightingShader.setUniform("u_time", time);
			m_lightingShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));

			// Color texture
			m_lightingShader.setUniform("t_colorTexture", 0);

			if (m_materialBlendIterations > 0) {
				m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
			} else {
				m_colorTexture.bind(0);
			}

			// Distance
			m_lightingShader.setUniform("t_distanceTexture", 1);
			m_distanceUpscaled.bind(1);

			// Background
			m_lightingShader.setUniform("t_backgroundTexture", 2);
			if (backgroundTexture) {
				backgroundTexture->bind(2);
			} else {
				m_backgroundTexture.bind(2);
			}

			// Corrected distance for shadows
			m_lightingShader.setUniform("t_shadowDistanceTexture", 3);
			if (m_config.useCorrectedDistance) {
				m_distanceTextureCorrected.bind(3);
			} else {
				m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(3);
			}

			m_renderPlane.draw(m_lightingShader);
		}

		void SDF2DRenderPipeline::handleDebugInputs()
		{
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::L))
			// {
			// 	m_2DLightingShader.toggleDefine("SHADOWS_ENABLED");
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::D))
			// {
			// 	if (Input::GetKey(Input::LeftShift)) {
			// 		m_finalUIShader.toggleDefine("DITHERING");
			// 	} else {
			// 		m_finalUIShader.toggleDefine("DEBUG_SHOW_DISTANCE");
			// 	}
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::C))
			// {
			// 	m_2DLightingShader.toggleDefine("DEBUG_SHOW_COLORS");
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::A))
			// {
			// 	m_2DLightingShader.toggleDefine("ANTIALIASING");
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::B))
			// {
			// 	m_2DDistanceShader.toggleDefine("BLEND_SHAPES");
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::M))
			// {
			// 	m_2DDistanceShader.toggleDefine("MOTION_BLUR");
			// 	m_uiDistanceShader.toggleDefine("MOTION_BLUR");
			// }
			//
			// if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::F))
			// {
			// 	USE_CORRECTED_DISTANCE_TEXTURE = !USE_CORRECTED_DISTANCE_TEXTURE;
			// }
		}

		Shader& SDF2DRenderPipeline::getDistanceShader()
		{
			return m_distanceShader;
		}
	}
}
