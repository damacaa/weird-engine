#pragma once

#include "weird-engine/vec.h"
#include "weird-renderer/core/RenderPlane.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DataBuffer.h"
#include "weird-renderer/resources/Shader.h"
#include "weird-renderer/resources/Texture.h"
#include "weird-renderer/scene/Camera.h"
#include "weird-renderer/scene/Light.h"
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class SDF3DRenderPipeline
		{
		public:
			struct Config
			{
				unsigned int renderWidth = 800;
				unsigned int renderHeight = 600;
				bool enablePathTracer = true;
				int maxAccumulationFrames = 100;
				int maxRayMarchSteps = 128;
				int rayBounces = 3;
				float rayMarchEpsilon = 0.001f;
				float maxRayDistance = 1000.0f;
			};

			SDF3DRenderPipeline(const Config& config, const glm::vec4* colorPalette, RenderPlane& renderPlane);
			~SDF3DRenderPipeline();

			Shader& getShader();

			// Renders the SDF 3D scene using ray marching with path-traced accumulation.
			// Leaves the output render target bound so geometry can be rendered on top.
			void render(
				vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
				const std::vector<Light>& lights,
				const Camera& camera,
				double time,
				Texture& geometryDepthTexture
			);

			RenderTarget& getRenderTarget();
			Texture& getOutputTexture();

			void resize(unsigned int newWidth, unsigned int newHeight);
			void free();
			void showDebugUI();

			Config& getConfig() { return m_config; }

		private:
			Config m_config;
			const glm::vec4* m_colorPalette;
			RenderPlane& m_renderPlane;

			static constexpr float NEAR_PLANE = 0.1f;
			static constexpr float FAR_PLANE = 300.0f;

			Shader m_sdfShader;
			DataBuffer* m_shapeDataBuffer;

			Texture m_outputTexture;
			Texture m_depthTexture;
			RenderTarget m_outputRender;

			// Double-buffered accumulation for path tracing
			Texture m_accumTexture[2];
			RenderTarget m_accumRender[2];
			int m_accumIdx;

			glm::mat4 m_oldCameraMatrix;
			int m_frameCounter;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
