#pragma once

#include "weird-engine/vec.h"
#include "weird-renderer/core/RenderPlane.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DataBuffer.h"
#include "weird-renderer/resources/Shader.h"
#include "weird-renderer/resources/Texture.h"
#include "weird-renderer/scene/Camera.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class SDF2DRenderPipeline
		{
		public:
			struct Config
			{
				unsigned int renderWidth;
				unsigned int renderHeight;
				float distanceSampleScale = 1.0f;
				float distanceOverscan =
					0.0f; // fraction added each side ([0.0..0.5]), e.g. 0.05 gives +5% world margin per side
				float renderScale = 1.0f;
				bool isUI; // UI mode: origin at bottom-left, [0,1] UV space, UI-specific distance clamping
				bool enableShadows;
				bool enableLongShadows = false;
				glm::vec3 shadowTint = glm::vec3(0.4f, 0.6f, 0.8f);
				bool enableRefraction;
				bool enableAntialiasing;
				bool enableMotionBlur;

				int materialBlendIterations;
				float materialBlendSpeed;
				float motionBlurBlendSpeed;
				bool debugDistanceField;
				bool debugMaterialColors;
				float ballK;
				float ambienOcclusionRadius;
				float ambienOcclusionStrength;
			};

			SDF2DRenderPipeline(const Config& config, const glm::vec4* colorPalette, RenderPlane& renderPlane);
			~SDF2DRenderPipeline();

			Shader& getDistanceShader();
			Texture& render(vec4* shapeData, uint32_t dataSize, uint32_t shapeCount, const Camera& camera, double time,
							double delta, Texture* backgroundTexture = nullptr);
			void resize(unsigned int newWidth, unsigned int newHeight);
			void free();
			void handleDebugInputs();

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

			Texture m_distanceTextureA;
			RenderTarget m_distanceRenderA;
			Texture m_distanceTextureB;
			RenderTarget m_distanceRenderB;
			RenderTarget* m_distanceTextureDoubleBuffer[2];
			int m_distanceTextureDoubleBufferIdx;

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
			glm::mat4
				m_prevFrameCameraMatrix; // snapshot of m_oldCameraMatrix taken before renderDistanceField overwrites it
			glm::vec3 m_lastCameraPosition;

			bool horizontal = true;
			glm::vec3 cameraPositionChange;

			void renderDistanceField(vec4* shapeData, uint32_t dataSize, uint32_t shapeCount, const Camera& camera,
									 double time, double delta);
			void applyJumpFloodCorrection(double time);
			void upscaleDistance();
			void renderMaterialColors(const Camera& camera, double time, double delta);
			void blendMaterials(double time);
			void renderBackground(const Camera& camera, double time);
			void applyLighting(const Camera& camera, double time, Texture* backgroundTexture);
			static int largestPowerOfTwoBelow(int n);
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
