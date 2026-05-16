#pragma once

#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/Shader.h"
#include "weird-renderer/resources/Texture.h"
#include "weird-renderer/scene/Camera.h"
#include "weird-renderer/scene/Light.h"
#include <vector>

// Forward declaration to avoid pulling in the full Scene header
namespace WeirdEngine
{
	class Scene;
}

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		// Handles rendering of 3D mesh geometry into a deferred GBuffer.
		// GBuffer layout:
		//   Attachment 0 (albedo)   : RGB = diffuse colour, A = specular intensity
		//   Attachment 1 (worldPos) : RGB = world-space position, A = 1 (valid pixel)
		//   Attachment 2 (normal)   : RGB = world-space normal (not remapped)
		//   Depth attachment        : standard 24-bit depth
		class MeshRenderPipeline
		{
		public:
			MeshRenderPipeline();
			~MeshRenderPipeline();

			Shader& getShader();
			Shader& getInstancedShader();

			// GBuffer accessors for the SDF3D pipeline.
			Texture& getGBufferAlbedo();
			Texture& getGBufferWorldPos();
			Texture& getGBufferNormal();
			Texture& getDepthTexture();

			// Renders 3D models into the internal GBuffer.
			// outputTarget is forwarded to Scene::onRender for custom per-scene rendering.
			void render(Scene& scene, RenderTarget& outputTarget, const Camera& camera,
						const std::vector<Light>& lights);

			// Recreates internal textures for a new resolution.
			void resize(unsigned int newWidth, unsigned int newHeight);
			void free();
			void showDebugUI();

		private:
			// Forward shaders (kept for compatibility / debug)
			Shader m_geometryShader;
			Shader m_instancedGeometryShader;

			// GBuffer shaders
			Shader m_gbufferShader;
			Shader m_gbufferInstancedShader;

			// GBuffer textures
			Texture m_gbufferAlbedo;
			Texture m_gbufferWorldPos;
			Texture m_gbufferNormal;
			Texture m_depthTexture;

			// Single FBO with 3 colour attachments + depth
			RenderTarget m_gbufferRender;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
