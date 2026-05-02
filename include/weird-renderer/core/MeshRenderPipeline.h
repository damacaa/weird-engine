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
		// Handles rendering of 3D mesh geometry (and eventually the draw queue).
		class MeshRenderPipeline
		{
		public:
			MeshRenderPipeline();
			~MeshRenderPipeline();

			Shader& getShader();
			Shader& getInstancedShader();

			// Returns the geometry depth texture for use by the SDF3D pipeline.
			Texture& getDepthTexture();

			// Renders 3D models into outputTarget using the provided camera and lights.
			void render(Scene& scene, RenderTarget& outputTarget, const Camera& camera,
						const std::vector<Light>& lights);

			// Recreates internal textures for a new resolution.
			void resize(unsigned int newWidth, unsigned int newHeight);
			void free();
			void showDebugUI();

		private:
			Shader m_geometryShader;
			Shader m_instancedGeometryShader;

			// Geometry depth buffer (for future pre-pass depth and SDF depth input).
			Texture m_depthTexture;
			RenderTarget m_depthRender;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
