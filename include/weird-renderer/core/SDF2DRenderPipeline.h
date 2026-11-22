#pragma once

#include "weird-engine/Background.h"
#include "weird-engine/Material2D.h"
#include "weird-engine/vec.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/core/Display.h"
#include "weird-renderer/core/RenderPlane.h"
#include "weird-renderer/core/RenderTarget.h"
#include "weird-renderer/resources/DataBuffer.h"
#include "weird-renderer/resources/Shader.h"
#include "weird-renderer/resources/Texture.h"
#include "weird-renderer/scene/Camera.h"
#include "weird-renderer/scene/Light.h"
#include <algorithm>
#include <vector>

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
				float refractionIntensity = 1.0f;
				bool enableAntialiasing;
				bool enableMotionBlur;

				int materialBlendIterations;
				float materialBlendSpeed;
				float motionBlurBlendSpeed = 10.0f;
				MotionBlurMethod motionBlurMethod = MotionBlurMethod::AsymmetricDelta;
				bool debugDistanceField;
				bool debugLightingDistanceField = false;
				bool debugMaterialColors;
				bool debugGrid = false;
				bool debugShapeTiles = false;
				float ballK;
				float ambienOcclusionRadius;
				float ambienOcclusionStrength;
			};

			SDF2DRenderPipeline(const Config& config, RenderPlane& renderPlane);
			~SDF2DRenderPipeline();

			Shader& getShapeShader();
			Shader& getDistanceShader();
			void resetCameraReprojection()
			{
				m_resetCameraReprojection = true;
			}
			void setBallK(float k)
			{
				float clamped = std::max(0.0f, k);
				bool wasEnabled = m_config.ballK > 0.0f;
				bool nowEnabled = clamped > 0.0f;
				if (nowEnabled != wasEnabled)
				{
					if (nowEnabled)
						m_distanceShader.addDefine("BLEND_SHAPES");
					else
						m_distanceShader.removeDefine("BLEND_SHAPES");
				}
				m_config.ballK = clamped;
			}
			float getBallK() const
			{
				return m_config.ballK;
			}
			Texture& render(const vec4* dotData, uint32_t dotCount, const vec4* shapeData,
							const Shape2DAABB* shapeBounds, uint32_t shapeCount, const std::vector<Light2D>& lights,
							const Material2D* materials, const Camera& camera, double time, double delta,
							const BackgroundParams& bgParams, Texture* backgroundTexture = nullptr);
			void resize(unsigned int newWidth, unsigned int newHeight);
			void free();
			void showDebugUI();

		private:
			Config m_config;
			RenderPlane& m_renderPlane;

			unsigned int m_distanceSampleWidth;
			unsigned int m_distanceSampleHeight;
			unsigned int m_materialBlendIterations;

			Shader m_shapeShader;
			Shader m_distanceShader;
			Shader m_jumpFloodInitShader;
			Shader m_jumpFloodStepShader;
			Shader m_distanceUpscalerShader;
			Shader m_materialColorShader;
			Shader m_materialBlendShader;
			Shader m_defaultBackgroundShader;
			Shader m_lightingShader;

			Texture m_shapeTexture;
			RenderTarget m_shapeRender;

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
			int m_lastFloodTextureIdx = 0;

			Texture m_distanceUpscaled;
			RenderTarget m_distanceUpscaler;

			Texture m_colorTexture;
			RenderTarget m_colorRender;

			Texture m_postProcessTextureFront;
			Texture m_postProcessTextureBack;
			RenderTarget m_postProcessRenderFront;
			RenderTarget m_postProcessRenderBack;
			RenderTarget* m_postProcessDoubleBuffer[2];

			Texture m_backgroundTextureFront;
			Texture m_backgroundTextureBack;
			RenderTarget m_backgroundRenderFront;
			RenderTarget m_backgroundRenderBack;
			RenderTarget* m_backgroundDoubleBuffer[2];
			int m_backgroundDoubleBufferIdx = 0;

			Texture m_litSceneTexture;
			RenderTarget m_litSceneRender;

			DataBuffer m_dotDataBuffer;
			DataBuffer m_shapeDataBuffer;
			DataBuffer m_shapeTileHeaderBuffer;
			DataBuffer m_shapeTileIndicesBuffer;
			DataBuffer m_gridHeaderBuffer;
			DataBuffer m_gridIndicesBuffer;

			// Reusable scratch buffers for dot grid building — allocated once, reused every frame
			struct ObjBounds
			{
				int minX, minY, maxX, maxY;
			};
			std::vector<ObjBounds> m_gridObjBounds;
			std::vector<int> m_gridCellCounts;
			std::vector<int> m_gridCellOffsets;
			std::vector<glm::vec4> m_gridHeader;
			std::vector<glm::vec4> m_gridIndices;

			// Reusable scratch buffers for 16x16 screen-space shape tile acceleration grid
			std::vector<vec2> m_shapePaddedMin;
			std::vector<vec2> m_shapePaddedMax;
			std::vector<ObjBounds> m_shapeTileObjBounds;
			std::vector<uint32_t> m_shapeTileCounts;
			std::vector<uint32_t> m_shapeTileOffsets;
			std::vector<uint32_t> m_shapeTileHeader;  // interleaved (start, count) pairs for GL_RG32UI
			std::vector<uint16_t> m_shapeTileIndices; // compact GL_R16UI shape index table

			glm::mat4 m_oldCameraMatrix;
			glm::mat4
				m_prevFrameCameraMatrix; // snapshot of m_oldCameraMatrix taken before renderDistanceField overwrites it
			glm::vec3 m_lastCameraPosition;

			bool horizontal = true;
			glm::vec3 cameraPositionChange = glm::vec3(0.0f);
			bool m_resetCameraReprojection = true;

			BackgroundType m_lastBackgroundType = (BackgroundType)-1;
			std::string m_lastBackgroundCustomCode = "";

			struct GridInfo
			{
				float minX = 0.0f, minY = 0.0f, stepX = 1.0f, stepY = 1.0f;
				float fadeStart = 0.0f, fadeSpan = 1.0f;
				int gridCols = 0, gridRows = 0, indexTexWidth = 0, indexTexHeight = 0;
				int visibleObjects = 0;
			};
			struct ShapeTileGridInfo
			{
				int tileCols, tileRows;
				int indexTexWidth, indexTexHeight;
				int visibleShapes;
				float fadeStart, fadeSpan, fadeReach;
			};
			ShapeTileGridInfo buildShapeTileGrid(const Shape2DAABB* shapeBounds, uint32_t shapeCount,
												 const Camera& camera);
			GridInfo buildAccelerationGrid(const vec4* dotData, uint32_t dotCount, const Camera& camera);
			void renderDistanceField(const vec4* dotData, uint32_t dotCount, const vec4* shapeData,
									 const Shape2DAABB* shapeBounds, uint32_t shapeCount, const Camera& camera,
									 double time, double delta);
			void applyJumpFloodCorrection(const Camera& camera);
			void upscaleDistance();
			void renderMaterialColors(const Material2D* materials, const Camera& camera, double time, double delta);
			void blendMaterials(double time);
			void renderBackground(const Camera& camera, double time, const BackgroundParams& bgParams);
			void applyLighting(const Material2D* materials, const std::vector<Light2D>& lights, const Camera& camera,
							   double time, Texture* backgroundTexture);
			static int largestPowerOfTwoBelow(int n);
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
