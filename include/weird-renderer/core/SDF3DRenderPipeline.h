#pragma once

#include "weird-engine/vec.h"
#include "weird-renderer/core/Display.h"
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
				float contrast = 1.2f;
				int maxRayMarchSteps = 128;
				int rayBounces = 3;
				float rayMarchEpsilon = 0.001f;
				float maxRayDistance = 1000.0f;
				bool enableAntialiasing = true;
			};

			SDF3DRenderPipeline(const Config& config, const glm::vec4* colorPalette, const DisplaySettings::ExtraMaterialData* materialDataPalette, RenderPlane& renderPlane);
			~SDF3DRenderPipeline();

			Shader& getShader();

			// Renders the SDF 3D scene using ray marching with path-traced accumulation.
			// GBuffer textures come from MeshRenderPipeline and allow the shader to composite
			// mesh surfaces with SDF lighting (SDFs cast light on meshes; meshes don't affect SDFs).
			void render(
				vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
				const std::vector<Light>& lights,
				const Camera& camera,
				double time,
				Texture& gbufferAlbedo,
				Texture& gbufferWorldPos,
				Texture& gbufferNormal,
				Texture& gbufferDepth
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
			const DisplaySettings::ExtraMaterialData* m_materialDataPalette;
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
