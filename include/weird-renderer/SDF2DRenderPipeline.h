#pragma once

#include "RenderTarget.h"
#include "DataBuffer.h"
#include "Shader.h"
#include "Texture.h"
#include "RenderPlane.h"
#include "Camera.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class SDF2DRenderPipeline
		{
		public:
			enum class DataMode
			{
				WORLD,
				UI
			};

			struct Config
			{
				unsigned int renderWidth;
				unsigned int renderHeight;
				float distanceSampleScale;
				float renderScale;
				DataMode dataMode;
				bool originAtBottomLeft;
				bool enableShadows;
				bool enableRefraction;
				bool enableAntialiasing;
				bool enableMotionBlur;
				bool enableDithering;
				bool useCorrectedDistance;
				int materialBlendIterations;
				float materialBlendSpeed;
			};

			SDF2DRenderPipeline(const Config& config, const glm::vec4* colorPalette, RenderPlane& renderPlane);
			~SDF2DRenderPipeline();

			Shader& getDistanceShader();
			Texture& render(WeirdRenderer::Dot2D* shapeData, uint32_t dataSize, const Camera& camera, double time, double delta, Texture* backgroundTexture = nullptr);
			void free();

		private:
			Config m_config;
			const glm::vec4* m_colorPalette;
			RenderPlane& m_renderPlane;

			unsigned int m_distanceSampleWidth;
			unsigned int m_distanceSampleHeight;
			unsigned int m_materialBlendIterations;

			Shader m_distanceShader;
			Shader m_jumpFloodInitShader;
			Shader m_jumpFloodStepShader;
			Shader m_distanceCorrectionShader;
			Shader m_distanceUpscalerShader;
			Shader m_materialColorShader;
			Shader m_materialBlendShader;
			Shader m_defaultBackgroundShader;
			Shader m_lightingShader;

			Texture m_distanceTexture;
			RenderTarget m_distanceRender;

			Texture m_jumpFloodInitTexture;
			RenderTarget m_jumpFloodInitRender;

			Texture m_jumpFloodTexturePing;
			Texture m_jumpFloodTexturePong;
			RenderTarget m_jumpFloodRenderPing;
			RenderTarget m_jumpFloodRenderPong;
			RenderTarget* m_jumpFloodDoubleBuffer[2];

			Texture m_distanceTextureCorrected;
			RenderTarget m_distanceCorrectionRender;

			Texture m_distanceUpscaled;
			RenderTarget m_distanceUpscaler;

			Texture m_colorTexture;
			RenderTarget m_colorRender;

			Texture m_postProcessTextureFront;
			Texture m_postProcessTextureBack;
			RenderTarget m_postProcessRenderFront;
			RenderTarget m_postProcessRenderBack;
			RenderTarget* m_postProcessDoubleBuffer[2];

			Texture m_backgroundTexture;
			RenderTarget m_backgroundRender;

			Texture m_litSceneTexture;
			RenderTarget m_litSceneRender;

			DataBuffer m_shapeDataBuffer;

			glm::mat4 m_oldCameraMatrix;
			glm::vec3 m_lastCameraPosition;

			bool horizontal = true;

			void renderDistanceField(WeirdRenderer::Dot2D* shapeData, uint32_t dataSize, const Camera& camera, double time, double delta);
			void applyJumpFloodCorrection(double time);
			void upscaleDistance();
			void renderMaterialColors(const Camera& camera, double time, double delta);
			void blendMaterials(double time);
			void renderBackground(const Camera& camera, double time);
			void applyLighting(const Camera& camera, double time, Texture* backgroundTexture);
			void handleDebugInputs();

			static int largestPowerOfTwoBelow(int n);
		};
	}
}
