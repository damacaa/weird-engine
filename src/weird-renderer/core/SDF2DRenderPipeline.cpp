#include "weird-renderer/core/SDF2DRenderPipeline.h"

#include <algorithm>

#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

#include "weird-audio/AudioEngine.h"
#include "weird-engine/Profiler.h"
#include "weird-engine/vec.h"

#include <glm/gtx/color_space.hpp>

#ifndef SHADERS_PATH
#define SHADERS_PATH
#endif // !SHADERS_PATH

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		static int largestPowerOfTwoBelow(int n)
		{
			int p = 1;
			while (p * 2 <= n)
			{
				p *= 2;
			}
			return p;
		}

		// Hints TBDR GPUs (e.g. ARM Mali) not to load stale color attachment contents from DRAM
		// before a fullscreen pass that overwrites every pixel.
		static inline void invalidateColorAttachment0()
		{
			if (glInvalidateFramebuffer)
			{
				static constexpr GLenum kColor0 = GL_COLOR_ATTACHMENT0;
				glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, &kColor0);
			}
		}

		int SDF2DRenderPipeline::largestPowerOfTwoBelow(int n)
		{
			return ::WeirdEngine::WeirdRenderer::largestPowerOfTwoBelow(n);
		}

		SDF2DRenderPipeline::SDF2DRenderPipeline(const Config& config, RenderPlane& renderPlane)
			: m_config(config)
			, m_renderPlane(renderPlane)
			, m_distanceSampleWidth(0)
			, m_distanceSampleHeight(0)
			, m_materialBlendIterations(config.materialBlendIterations)
			, m_oldCameraMatrix(1.0f)
			, m_prevFrameCameraMatrix(1.0f)
			, m_lastCameraPosition(0.0f)
			, m_resetCameraReprojection(true)
		{
			float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);
			float overscanScale = 1.0f + 2.0f * overscan;
			m_distanceSampleWidth = (unsigned int)(m_config.renderWidth * m_config.distanceSampleScale * overscanScale);
			m_distanceSampleHeight =
				(unsigned int)(m_config.renderHeight * m_config.distanceSampleScale * overscanScale);

			// Load shaders
			m_shapeShader = Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/sdf_shapes.frag");
			if (m_config.isUI)
			{
				m_shapeShader.addDefine("UI_PIPELINE");
			}

			m_distanceShader = Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/sdf_dots.frag");

			if (config.ballK > 0.0f)
				m_distanceShader.addDefine("BLEND_SHAPES");

			if (m_config.enableMotionBlur)
			{
				m_distanceShader.addDefine("MOTION_BLUR");
				if (m_config.motionBlurMethod == MotionBlurMethod::FillOverride)
				{
					m_distanceShader.addDefine("MOTION_BLUR_FILL_OVERRIDE");
				}
			}

			if (m_config.isUI)
			{
				m_distanceShader.addDefine("UI_PIPELINE");
			}

			m_jumpFloodInitShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/jump_flood_init.frag");
			m_jumpFloodStepShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/jump_flood_step.frag");
			m_distanceUpscalerShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/distance_upscaler.frag");
			m_materialColorShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/material_color.frag");
			if (m_config.materialBlendIterations > 0)
			{
				m_materialColorShader.addDefine("MATERIAL_BLENDING");
			}

			m_materialBlendShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/material_blend.frag");
			m_defaultBackgroundShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/background.frag");

			m_lightingShader = Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/lighting.frag");
			if (m_config.enableShadows)
			{
				m_lightingShader.addDefine("SHADOWS_ENABLED");
			}
			if (m_config.enableLongShadows)
			{
				m_lightingShader.addDefine("LONG_SHADOWS");
			}
			if (m_config.enableRefraction)
			{
				m_lightingShader.addDefine("REFRACTION");
			}
			if (m_config.enableAntialiasing)
			{
				m_lightingShader.addDefine("ANTIALIASING");
			}

			if (m_config.debugDistanceField)
			{
				m_lightingShader.addDefine("DEBUG_SHOW_DISTANCE");
			}
			if (m_config.debugMaterialColors)
			{
				m_lightingShader.addDefine("DEBUG_SHOW_COLORS");
			}

			// Initialize textures and render targets
			m_shapeTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_shapeRender = RenderTarget(false);
			m_shapeRender.bindColorTextureToFrameBuffer(m_shapeTexture);

			m_distanceTextureA =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderA = RenderTarget(false);
			m_distanceRenderA.bindColorTextureToFrameBuffer(m_distanceTextureA);

			m_distanceTextureB =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderB = RenderTarget(false);
			m_distanceRenderB.bindColorTextureToFrameBuffer(m_distanceTextureB);

			m_distanceTextureDoubleBuffer[0] = &m_distanceRenderA;
			m_distanceTextureDoubleBuffer[1] = &m_distanceRenderB;
			m_distanceTextureDoubleBufferIdx = 0;

			// Clear double-buffered distance textures to a large positive value (10.0f)
			// to avoid motion-blur blending artifacts from uninitialized (0.0) pixels.
			m_distanceRenderA.bind();
			glClearColor(10.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_distanceRenderB.bind();
			glClearColor(10.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			m_jumpFloodInitTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_jumpFloodInitRender = RenderTarget(false);
			m_jumpFloodInitRender.bindColorTextureToFrameBuffer(m_jumpFloodInitTexture);

			// LinearData: the flood steps read these via texelFetch (filter-agnostic), while lighting samples the
			// final flood texture bilinearly so shadows stay smooth below the distance sample resolution
			m_jumpFloodTexturePing =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodTexturePong =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodRenderPing = RenderTarget(false);
			m_jumpFloodRenderPing.bindColorTextureToFrameBuffer(m_jumpFloodTexturePing);
			m_jumpFloodRenderPong = RenderTarget(false);
			m_jumpFloodRenderPong.bindColorTextureToFrameBuffer(m_jumpFloodTexturePong);
			m_jumpFloodDoubleBuffer[0] = &m_jumpFloodRenderPing;
			m_jumpFloodDoubleBuffer[1] = &m_jumpFloodRenderPong;

			m_distanceUpscaled = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_distanceUpscaler = RenderTarget(false);
			m_distanceUpscaler.bindColorTextureToFrameBuffer(m_distanceUpscaled);

			m_colorTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_colorRender = RenderTarget(false);
			m_colorRender.bindColorTextureToFrameBuffer(m_colorTexture);

			m_postProcessTextureFront =
				Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderFront = RenderTarget(false);
			m_postProcessRenderFront.bindColorTextureToFrameBuffer(m_postProcessTextureFront);

			m_postProcessTextureBack = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderBack = RenderTarget(false);
			m_postProcessRenderBack.bindColorTextureToFrameBuffer(m_postProcessTextureBack);

			m_postProcessDoubleBuffer[0] = &m_postProcessRenderFront;
			m_postProcessDoubleBuffer[1] = &m_postProcessRenderBack;

			m_backgroundTextureFront =
				Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundTextureBack = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundRenderFront = RenderTarget(false);
			m_backgroundRenderBack = RenderTarget(false);
			m_backgroundRenderFront.bindColorTextureToFrameBuffer(m_backgroundTextureFront);
			m_backgroundRenderBack.bindColorTextureToFrameBuffer(m_backgroundTextureBack);
			m_backgroundDoubleBuffer[0] = &m_backgroundRenderFront;
			m_backgroundDoubleBuffer[1] = &m_backgroundRenderBack;
			m_backgroundDoubleBufferIdx = 0;

			m_litSceneTexture =
				Texture(m_config.renderWidth, m_config.renderHeight,
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
			m_shapeShader.free();
			m_distanceShader.free();
			m_jumpFloodInitShader.free();
			m_jumpFloodStepShader.free();
			m_distanceUpscalerShader.free();
			m_materialColorShader.free();
			m_materialBlendShader.free();
			m_defaultBackgroundShader.free();
			m_lightingShader.free();

			m_shapeRender.free();
			m_distanceRenderA.free();
			m_distanceRenderB.free();
			m_jumpFloodInitRender.free();
			m_jumpFloodRenderPing.free();
			m_jumpFloodRenderPong.free();
			m_distanceUpscaler.free();
			m_colorRender.free();
			m_postProcessRenderFront.free();
			m_postProcessRenderBack.free();

			m_backgroundRenderFront.free();
			m_backgroundRenderBack.free();

			m_litSceneRender.free();

			m_shapeTexture.dispose();
			m_distanceTextureA.dispose();
			m_distanceTextureB.dispose();
			m_jumpFloodInitTexture.dispose();
			m_jumpFloodTexturePing.dispose();
			m_jumpFloodTexturePong.dispose();
			m_distanceUpscaled.dispose();
			m_colorTexture.dispose();
			m_postProcessTextureFront.dispose();
			m_postProcessTextureBack.dispose();
			m_backgroundTextureFront.dispose();
			m_backgroundTextureBack.dispose();
			m_litSceneTexture.dispose();
		}

		void SDF2DRenderPipeline::resize(unsigned int newWidth, unsigned int newHeight)
		{
			m_config.renderWidth = newWidth;
			m_config.renderHeight = newHeight;
			float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);
			float overscanScale = 1.0f + 2.0f * overscan;
			m_distanceSampleWidth = (unsigned int)(m_config.renderWidth * m_config.distanceSampleScale * overscanScale);
			m_distanceSampleHeight =
				(unsigned int)(m_config.renderHeight * m_config.distanceSampleScale * overscanScale);

			// Dispose old textures
			m_shapeTexture.dispose();
			m_distanceTextureA.dispose();
			m_distanceTextureB.dispose();
			m_jumpFloodInitTexture.dispose();
			m_jumpFloodTexturePing.dispose();
			m_jumpFloodTexturePong.dispose();
			m_distanceUpscaled.dispose();
			m_colorTexture.dispose();
			m_postProcessTextureFront.dispose();
			m_postProcessTextureBack.dispose();
			m_backgroundTextureFront.dispose();
			m_backgroundTextureBack.dispose();
			m_litSceneTexture.dispose();

			// Recreate textures with new dimensions and rebind to existing render targets
			m_shapeTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_shapeRender.bindColorTextureToFrameBuffer(m_shapeTexture);

			m_distanceTextureA =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderA.bindColorTextureToFrameBuffer(m_distanceTextureA);

			m_distanceTextureB =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderB.bindColorTextureToFrameBuffer(m_distanceTextureB);

			// Clear double-buffered distance textures to a large positive value (10.0f)
			// to avoid motion-blur blending artifacts from uninitialized (0.0) pixels.
			m_distanceRenderA.bind();
			glClearColor(10.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			m_distanceRenderB.bind();
			glClearColor(10.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			m_jumpFloodInitTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_jumpFloodInitRender.bindColorTextureToFrameBuffer(m_jumpFloodInitTexture);

			m_jumpFloodTexturePing =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodRenderPing.bindColorTextureToFrameBuffer(m_jumpFloodTexturePing);

			m_jumpFloodTexturePong =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_jumpFloodRenderPong.bindColorTextureToFrameBuffer(m_jumpFloodTexturePong);

			m_distanceUpscaled = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_distanceUpscaler.bindColorTextureToFrameBuffer(m_distanceUpscaled);

			m_colorTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_colorRender.bindColorTextureToFrameBuffer(m_colorTexture);

			m_postProcessTextureFront =
				Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderFront.bindColorTextureToFrameBuffer(m_postProcessTextureFront);

			m_postProcessTextureBack = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Data);
			m_postProcessRenderBack.bindColorTextureToFrameBuffer(m_postProcessTextureBack);

			m_backgroundTextureFront =
				Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundTextureBack = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundRenderFront.bindColorTextureToFrameBuffer(m_backgroundTextureFront);
			m_backgroundRenderBack.bindColorTextureToFrameBuffer(m_backgroundTextureBack);

			m_litSceneTexture =
				Texture(m_config.renderWidth, m_config.renderHeight,
						m_config.renderScale <= 0.5f ? Texture::TextureType::RetroColor : Texture::TextureType::Data);
			m_litSceneRender.bindColorTextureToFrameBuffer(m_litSceneTexture);
		}

		Texture& SDF2DRenderPipeline::render(const vec4* dotData, uint32_t dotCount, const vec4* shapeData,
											 const Shape2DAABB* shapeBounds, uint32_t shapeCount,
											 const std::vector<Light2D>& lights, const Material2D* materials,
											 const Camera& camera, double time, double delta,
											 const BackgroundParams& bgParams, Texture* backgroundTexture)
		{
			// PROFILE_SCOPE(m_config.isUI ? "SDF2DRenderPipeline (UI)" : "SDF2DRenderPipeline (World)");

			// Execute all pipeline stages
			renderDistanceField(dotData, dotCount, shapeData, shapeBounds, shapeCount, camera, time, delta);

			if (m_config.enableShadows && m_config.enableLongShadows)
			{
				applyJumpFloodCorrection(camera); // Produces the flood texture consumed by lighting for long shadows/AO
			}

			upscaleDistance();
			renderMaterialColors(materials, camera, time, delta);
			blendMaterials(time);
			if (!backgroundTexture)
			{
				renderBackground(camera, time, bgParams);
			}

			// Dynamically set shadow tint based on background primary color to simulate global sky ambient.
			// glm::hsvColor() leaves hue undefined for achromatic colors, so neutral gray backgrounds can produce
			// NaNs that poison the final lighting mix even when shadows are disabled.
			glm::vec3 backgroundRgb = glm::vec3(bgParams.primaryColor);
			float maxChannel = std::max(backgroundRgb.r, std::max(backgroundRgb.g, backgroundRgb.b));
			float minChannel = std::min(backgroundRgb.r, std::min(backgroundRgb.g, backgroundRgb.b));
			glm::vec3 hsvAmbient =
				maxChannel - minChannel > 1e-6f ? glm::hsvColor(backgroundRgb) : glm::vec3(0.0f, 0.0f, maxChannel);

			// Shift hue slightly (e.g. +15 degrees) to make shadows more interesting
			hsvAmbient.x += 15.0f;
			if (hsvAmbient.x >= 360.0f)
				hsvAmbient.x -= 360.0f;

			// Increase saturation to get a pure, vibrant color tone
			hsvAmbient.y = std::min(hsvAmbient.y * 1.5f + 0.2f, 1.0f);
			hsvAmbient.z = 1.0f;

			glm::vec3 vibrantColor = glm::rgbColor(hsvAmbient);

			// Map the vibrant color to a valid transmittance range.
			// We want the shadow to block at most ~25% of light (0.75 transmittance)
			// to keep it from getting too dark, while fully letting the tinted color through.
			glm::vec3 shadowTransmittance = glm::mix(glm::vec3(0.75f), glm::vec3(1.0f), vibrantColor);

			m_config.shadowTint = shadowTransmittance;

			applyLighting(materials, lights, camera, time, backgroundTexture);

			Profiler::get().gpuSync(); // Stalls if recording average report

			return m_litSceneTexture;
		}

		SDF2DRenderPipeline::GridInfo SDF2DRenderPipeline::buildAccelerationGrid(const vec4* dotData, uint32_t dotCount,
																				 const Camera& camera)
		{
			// Build acceleration grid using CPU padding spatial partitioning.
			// Grid bounds are derived entirely from the camera frustum, so off-screen circles are
			// never inserted. This relies on the 2D camera convention where:
			//   zoom  = -view[3].z   (positive distance from origin)
			//   camXY = view[3].xy   (camera translation in world space)
			// If the camera representation changes, update the frustum derivation below accordingly.
			constexpr int indexTexWidth = 64;

			int objectCount = static_cast<int>(dotCount);

			// Derive the visible world-space rectangle from the camera matrix
			float zoom = -camera.view[3].z;
			float absZoom = std::abs(zoom);
			float camX = camera.view[3].x;
			float camY = camera.view[3].y;
			float aspectRatio = static_cast<float>(m_distanceSampleWidth) / static_cast<float>(m_distanceSampleHeight);
			float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);

			float uvXMin, uvYMin, uvXMax, uvYMax;
			if (m_config.isUI)
			{
				uvXMin = 0.0f;
				uvYMin = 0.0f;
				uvXMax = 1.0f * aspectRatio;
				uvYMax = 1.0f;
			}
			else
			{
				uvXMin = (-1.0f - overscan) * aspectRatio;
				uvYMin = (-1.0f - overscan);
				uvXMax = (1.0f + overscan) * aspectRatio;
				uvYMax = (1.0f + overscan);
			}

			float viewMinX = zoom * uvXMin - camX;
			float viewMinY = zoom * uvYMin - camY;
			float viewMaxX = zoom * uvXMax - camX;
			float viewMaxY = zoom * uvYMax - camY;
			if (viewMinX > viewMaxX)
				std::swap(viewMinX, viewMaxX);
			if (viewMinY > viewMaxY)
				std::swap(viewMinY, viewMaxY);

			// Compute world-space texel, AA smoothing, AO, and zoom distance bonus bounds so grid
			// cell boundaries never clip dots and the distance field saturates C2-smoothly before cell edges.
			float worldTexelX = (viewMaxX - viewMinX) / static_cast<float>(std::max(1u, m_distanceSampleWidth));
			float worldTexelY = (viewMaxY - viewMinY) / static_cast<float>(std::max(1u, m_distanceSampleHeight));
			float worldTexel = std::max(worldTexelX, worldTexelY);

			float distanceBonus = 0.0f;
			float worldSmoothing = 0.0f;
			if (!m_config.isUI)
			{
				if (m_distanceSampleHeight > 0)
				{
					distanceBonus =
						std::min(0.00002f * zoom * zoom, 1.0f * absZoom / static_cast<float>(m_distanceSampleHeight));
				}
				if (m_config.renderHeight > 0)
				{
					float maxSmoothingNorm = 2.0f * (1.0f + overscan) / static_cast<float>(m_config.renderHeight);
					float aoWorld = (!m_config.enableShadows || !m_config.enableLongShadows)
										? (absZoom * m_config.ambienOcclusionRadius * 0.001f)
										: 0.0f;
					worldSmoothing = std::max(maxSmoothingNorm * 2.0f * absZoom * aspectRatio, aoWorld);
				}
			}

			float dotRadius = m_config.isUI ? 5.0f : 0.5f;
			float safetyMargin = m_config.isUI ? 1.0f : 0.5f;
			float fadeStart = 0.0f;
			float fadeReach = 0.0f;
			if (m_config.isUI)
			{
				fadeStart = std::min(6.0f, std::max(2.0f, worldTexel * 2.0f + safetyMargin));
				fadeReach = 10.0f;
			}
			else
			{
				fadeStart = worldSmoothing + 2.0f * worldTexel + safetyMargin;
				float minSpan = std::max(1.5f, 4.0f * worldTexel);
				fadeReach = std::max({2.5f, fadeStart + 0.5f * minSpan, 0.06f * absZoom});
			}
			// Integral of 1 - smoothstep(0, 1, t) reaches plateau at t = 1 with height 0.5 * fadeSpan.
			float fadeSpan = 2.0f * (fadeReach - fadeStart);
			float smoothReach = std::max(0.0f, m_config.ballK);
			float cellPad = dotRadius + fadeStart + fadeSpan + distanceBonus + smoothReach + safetyMargin;

			// Expand frustum by cellPad so circles that straddle the view edge are included
			float minX = viewMinX - cellPad;
			float minY = viewMinY - cellPad;
			float maxX = viewMaxX + cellPad;
			float maxY = viewMaxY + cellPad;

			// Base cell size keeps a dot in ~2x2 cells at close zoom while scaling with world texel
			// size and cellPad when zoomed far out so header memory and per-dot cell counts stay bounded.
			float baseCellSize = m_config.isUI ? 10.0f : 2.0f;
			float idealCellSize = std::max({baseCellSize, worldTexel, cellPad * 0.5f});
			if (idealCellSize < 0.5f)
				idealCellSize = 0.5f;
			int gridCols = static_cast<int>(std::ceil((maxX - minX) / idealCellSize));
			int gridRows = static_cast<int>(std::ceil((maxY - minY) / idealCellSize));

			if (gridCols < 1)
				gridCols = 1;
			if (gridRows < 1)
				gridRows = 1;
			if (gridCols > 2048)
				gridCols = 2048;
			if (gridRows > 2048)
				gridRows = 2048;

			float stepX = (maxX - minX) / gridCols;
			float stepY = (maxY - minY) / gridRows;

			int numCells = gridCols * gridRows;

			// Reuse member scratch buffers — no heap allocation unless the scene grows
			m_gridObjBounds.clear();
			m_gridObjBounds.resize(objectCount);
			m_gridCellCounts.assign(numCells, 0);

			// Single pass: compute per-object cell ranges and tally cell counts.
			// Objects whose cell range is entirely outside [0, gridCols/Rows) are naturally skipped.
			int visibleObjects = 0;
			for (int i = 0; i < objectCount; ++i)
			{
				float cx = dotData[i].x;
				float cy = dotData[i].y;
				int minCellX = static_cast<int>(std::floor((cx - cellPad - minX) / stepX));
				int minCellY = static_cast<int>(std::floor((cy - cellPad - minY) / stepY));
				int maxCellX = static_cast<int>(std::floor((cx + cellPad - minX) / stepX));
				int maxCellY = static_cast<int>(std::floor((cy + cellPad - minY) / stepY));

				// Cull objects entirely outside the frustum-aligned grid
				if (maxCellX < 0 || maxCellY < 0 || minCellX >= gridCols || minCellY >= gridRows)
				{
					m_gridObjBounds[i] = {-1, -1, -1, -1};
					continue;
				}

				minCellX = std::max(0, minCellX);
				minCellY = std::max(0, minCellY);
				maxCellX = std::min(gridCols - 1, maxCellX);
				maxCellY = std::min(gridRows - 1, maxCellY);

				m_gridObjBounds[i] = {minCellX, minCellY, maxCellX, maxCellY};
				++visibleObjects;

				for (int cyI = minCellY; cyI <= maxCellY; ++cyI)
					for (int cxI = minCellX; cxI <= maxCellX; ++cxI)
						m_gridCellCounts[cyI * gridCols + cxI]++;
			}

			// Prefix sum to compute cell offsets
			m_gridCellOffsets.resize(numCells);
			int totalIndices = 0;
			for (int i = 0; i < numCells; ++i)
			{
				m_gridCellOffsets[i] = totalIndices;
				totalIndices += m_gridCellCounts[i];
			}

			// Keep height >= 2 so Mali Bifrost drivers never hit degenerate 1-row 2D texture paths.
			int indexTexHeight = std::max(2, (totalIndices + indexTexWidth - 1) / indexTexWidth);
			int paddedSize = indexTexWidth * indexTexHeight;

			m_gridHeader.resize(numCells);
			m_gridIndices.assign(paddedSize, glm::vec4(0.0f));

			// Fill index buffer using offset-as-write-cursor directly (no extra copy)
			for (int i = 0; i < objectCount; ++i)
			{
				const ObjBounds& b = m_gridObjBounds[i];
				if (b.minX < 0)
					continue; // culled
				for (int cyI = b.minY; cyI <= b.maxY; ++cyI)
				{
					for (int cxI = b.minX; cxI <= b.maxX; ++cxI)
					{
						int cellIdx = cyI * gridCols + cxI;
						m_gridIndices[m_gridCellOffsets[cellIdx]++] =
							glm::vec4(static_cast<float>(i), 0.0f, 0.0f, 0.0f);
					}
				}
			}

			// Restore offsets for header (they were incremented above as write cursors)
			int writePos = 0;
			for (int i = 0; i < numCells; ++i)
			{
				m_gridHeader[i] =
					glm::vec4(static_cast<float>(writePos), static_cast<float>(m_gridCellCounts[i]), 0.0f, 0.0f);
				writePos += m_gridCellCounts[i];
			}

			return {minX,	  minY,		stepX,		   stepY,		   fadeStart,	  fadeSpan,
					gridCols, gridRows, indexTexWidth, indexTexHeight, visibleObjects};
		}

		SDF2DRenderPipeline::ShapeTileGridInfo SDF2DRenderPipeline::buildShapeTileGrid(const Shape2DAABB* shapeBounds,
																					   uint32_t shapeCount,
																					   const Camera& camera)
		{
			constexpr int TILE_SIZE = 16;
			constexpr int INDEX_TEX_WIDTH = 64;
			constexpr float kNegInf = -std::numeric_limits<float>::infinity();
			constexpr float kPosInf = std::numeric_limits<float>::infinity();

			const int sampleW = static_cast<int>(std::max(1u, m_distanceSampleWidth));
			const int sampleH = static_cast<int>(std::max(1u, m_distanceSampleHeight));
			const int tileCols = std::max(1, (sampleW + TILE_SIZE - 1) / TILE_SIZE);
			const int tileRows = std::max(1, (sampleH + TILE_SIZE - 1) / TILE_SIZE);
			const int numTiles = tileCols * tileRows;

			// Derive visible world rectangle matching sdf_shapes.frag coordinate mapping
			const float zoom = -camera.view[3].z;
			const float absZoom = std::abs(zoom);
			const float camX = camera.view[3].x;
			const float camY = camera.view[3].y;
			const float aspectRatio = static_cast<float>(sampleW) / static_cast<float>(sampleH);
			const float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);

			float uvX0, uvY0, uvX1, uvY1;
			if (m_config.isUI)
			{
				uvX0 = 0.0f;
				uvY0 = 0.0f;
				uvX1 = aspectRatio;
				uvY1 = 1.0f;
			}
			else
			{
				uvX0 = (-1.0f - overscan) * aspectRatio;
				uvY0 = (-1.0f - overscan);
				uvX1 = (1.0f + overscan) * aspectRatio;
				uvY1 = (1.0f + overscan);
			}

			const float worldX0 = zoom * uvX0 - camX;
			const float worldY0 = zoom * uvY0 - camY;
			const float worldX1 = zoom * uvX1 - camX;
			const float worldY1 = zoom * uvY1 - camY;
			const float worldPerPxX = (worldX1 - worldX0) / static_cast<float>(sampleW);
			const float worldPerPxY = (worldY1 - worldY0) / static_cast<float>(sampleH);
			const float worldTexel = std::max(std::abs(worldPerPxX), std::abs(worldPerPxY));

			// Compute C2 saturation range & conservative exterior padding so tile boundaries are invisible
			float worldSmoothing = 0.0f;
			if (!m_config.isUI && m_config.renderHeight > 0)
			{
				const float maxSmoothingNorm = 2.0f * (1.0f + overscan) / static_cast<float>(m_config.renderHeight);
				const float aoWorld = (!m_config.enableShadows || !m_config.enableLongShadows)
										  ? (absZoom * m_config.ambienOcclusionRadius * 0.001f)
										  : 0.0f;
				worldSmoothing = std::max(maxSmoothingNorm * 2.0f * absZoom * aspectRatio, aoWorld);
			}

			float fadeStart = 0.0f;
			float fadeReach = 10.0f;
			if (m_config.isUI)
			{
				fadeStart = std::min(6.0f, std::max(2.0f, worldTexel * 2.0f + 1.0f));
				fadeReach = 10.0f;
			}
			else
			{
				fadeStart = worldSmoothing + 2.0f * worldTexel + 0.5f;
				const float minSpan = std::max(1.5f, 4.0f * worldTexel);
				fadeReach = std::max({2.5f, fadeStart + 0.5f * minSpan, 0.06f * absZoom});
			}
			const float fadeSpan = 2.0f * (fadeReach - fadeStart);
			const float basePad = fadeStart + fadeSpan + worldTexel + 0.5f;

			// Clamp shapeCount to uint16_t index capacity
			const uint32_t count = std::min<uint32_t>(shapeCount, 65535u);

			m_shapePaddedMin.resize(count);
			m_shapePaddedMax.resize(count);
			m_shapeTileObjBounds.resize(count);
			m_shapeTileCounts.assign(numTiles, 0u);

			// 1. Compute global smooth radius from GLOBAL_LAYER (if present at tail of sorted shape list)
			float globalMaxSmooth = 0.0f;
			uint32_t globalStart = count;
			if (shapeBounds)
			{
				for (uint32_t i = count; i > 0; --i)
				{
					const Shape2DAABB& b = shapeBounds[i - 1];
					if (b.layer != Shape::GLOBAL_LAYER)
					{
						break;
					}
					globalStart = i - 1;
					if (b.combination == CombinationType::SmoothAddition ||
						b.combination == CombinationType::SmoothSubtraction)
					{
						globalMaxSmooth = std::max(globalMaxSmooth, std::max(b.smoothFactor, 0.0f));
					}
				}
			}

			auto expandAxisMin = [](float v, float pad) -> float { return std::isfinite(v) ? (v - pad) : v; };
			auto expandAxisMax = [](float v, float pad) -> float { return std::isfinite(v) ? (v + pad) : v; };

			// 2. Per-layer padding + backward Intersection AABB clamping (preserves exact CSG semantics!)
			vec2 globalClipMin(kNegInf, kNegInf);
			vec2 globalClipMax(kPosInf, kPosInf);
			bool hasGlobalClip = false;

			if (shapeBounds)
			{
				uint32_t lStart = 0;
				while (lStart < count)
				{
					const uint16_t layerId = shapeBounds[lStart].layer;
					uint32_t lEnd = lStart + 1;
					float layerMaxSmooth = 0.0f;
					while (lEnd < count && shapeBounds[lEnd].layer == layerId)
					{
						++lEnd;
					}

					for (uint32_t i = lStart; i < lEnd; ++i)
					{
						const auto comb = shapeBounds[i].combination;
						if (comb == CombinationType::SmoothAddition || comb == CombinationType::SmoothSubtraction)
						{
							layerMaxSmooth = std::max(layerMaxSmooth, std::max(shapeBounds[i].smoothFactor, 0.0f));
						}
					}

					const float layerPad = basePad + layerMaxSmooth + globalMaxSmooth;
					for (uint32_t i = lStart; i < lEnd; ++i)
					{
						m_shapePaddedMin[i] = vec2(expandAxisMin(shapeBounds[i].min.x, layerPad),
												   expandAxisMin(shapeBounds[i].min.y, layerPad));
						m_shapePaddedMax[i] = vec2(expandAxisMax(shapeBounds[i].max.x, layerPad),
												   expandAxisMax(shapeBounds[i].max.y, layerPad));
					}

					// Backward pass: an Intersection clamps all preceding shapes in the same layer
					vec2 clipMin(kNegInf, kNegInf);
					vec2 clipMax(kPosInf, kPosInf);
					bool hasClip = false;
					for (uint32_t i = lEnd; i > lStart; --i)
					{
						const uint32_t idx = i - 1;
						if (hasClip)
						{
							m_shapePaddedMin[idx] = glm::max(m_shapePaddedMin[idx], clipMin);
							m_shapePaddedMax[idx] = glm::min(m_shapePaddedMax[idx], clipMax);
						}
						if (shapeBounds[idx].combination == CombinationType::Intersection)
						{
							clipMin = glm::max(clipMin, m_shapePaddedMin[idx]);
							clipMax = glm::min(clipMax, m_shapePaddedMax[idx]);
							hasClip = true;
						}
					}

					if (layerId == Shape::GLOBAL_LAYER && hasClip)
					{
						globalClipMin = clipMin;
						globalClipMax = clipMax;
						hasGlobalClip = true;
					}

					lStart = lEnd;
				}

				if (hasGlobalClip)
				{
					for (uint32_t i = 0; i < globalStart; ++i)
					{
						m_shapePaddedMin[i] = glm::max(m_shapePaddedMin[i], globalClipMin);
						m_shapePaddedMax[i] = glm::min(m_shapePaddedMax[i], globalClipMax);
					}
				}
			}
			else
			{
				for (uint32_t i = 0; i < count; ++i)
				{
					m_shapePaddedMin[i] = vec2(kNegInf, kNegInf);
					m_shapePaddedMax[i] = vec2(kPosInf, kPosInf);
				}
			}

			// 3. Project padded AABBs onto 16x16 screen tiles and tally counts
			const float tileWorldW = worldPerPxX * static_cast<float>(TILE_SIZE);
			const float tileWorldH = worldPerPxY * static_cast<float>(TILE_SIZE);
			const bool validProjection = (tileWorldW > 1e-9f && tileWorldH > 1e-9f);

			auto worldToTileClamped = [](float worldCoord, float origin, float tileSpan, int maxTile) -> int
			{
				if (std::isnan(worldCoord))
					return 0;
				if (std::isinf(worldCoord))
					return worldCoord < 0.0f ? -1 : maxTile;
				const float t = std::floor((worldCoord - origin) / tileSpan);
				if (t < -1.0f)
					return -1;
				if (t > static_cast<float>(maxTile))
					return maxTile;
				return static_cast<int>(t);
			};

			int visibleShapes = 0;
			for (uint32_t i = 0; i < count; ++i)
			{
				const vec2& mn = m_shapePaddedMin[i];
				const vec2& mx = m_shapePaddedMax[i];

				if (mn.x > mx.x || mn.y > mx.y)
				{
					m_shapeTileObjBounds[i] = {-1, -1, -1, -1};
					continue;
				}

				int minTX = 0;
				int maxTX = tileCols - 1;
				int minTY = 0;
				int maxTY = tileRows - 1;

				if (validProjection)
				{
					minTX = worldToTileClamped(mn.x, worldX0, tileWorldW, tileCols);
					maxTX = worldToTileClamped(mx.x, worldX0, tileWorldW, tileCols);
					minTY = worldToTileClamped(mn.y, worldY0, tileWorldH, tileRows);
					maxTY = worldToTileClamped(mx.y, worldY0, tileWorldH, tileRows);

					if (maxTX < 0 || maxTY < 0 || minTX >= tileCols || minTY >= tileRows)
					{
						m_shapeTileObjBounds[i] = {-1, -1, -1, -1};
						continue;
					}

					minTX = std::max(0, minTX);
					minTY = std::max(0, minTY);
					maxTX = std::min(tileCols - 1, maxTX);
					maxTY = std::min(tileRows - 1, maxTY);
				}

				m_shapeTileObjBounds[i] = {minTX, minTY, maxTX, maxTY};
				++visibleShapes;

				for (int ty = minTY; ty <= maxTY; ++ty)
				{
					const int rowBase = ty * tileCols;
					for (int tx = minTX; tx <= maxTX; ++tx)
					{
						m_shapeTileCounts[rowBase + tx]++;
					}
				}
			}

			// 4. Prefix sum offsets & fill compact uint16_t tile index table (preserves sorted layer/CSG order)
			m_shapeTileOffsets.resize(numTiles);
			m_shapeTileHeader.resize(static_cast<size_t>(numTiles) * 2);
			uint32_t totalIndices = 0;
			for (int t = 0; t < numTiles; ++t)
			{
				const uint32_t c = m_shapeTileCounts[t];
				m_shapeTileOffsets[t] = totalIndices;
				m_shapeTileHeader[static_cast<size_t>(t) * 2] = totalIndices;
				m_shapeTileHeader[static_cast<size_t>(t) * 2 + 1] = c;
				totalIndices += c;
			}

			const int indexTexHeight =
				std::max(2, static_cast<int>((totalIndices + INDEX_TEX_WIDTH - 1) / INDEX_TEX_WIDTH));
			const size_t paddedIndices = static_cast<size_t>(INDEX_TEX_WIDTH) * static_cast<size_t>(indexTexHeight);
			m_shapeTileIndices.assign(paddedIndices, 0u);

			for (uint32_t i = 0; i < count; ++i)
			{
				const ObjBounds& b = m_shapeTileObjBounds[i];
				if (b.minX < 0)
					continue;
				const uint16_t shapeIdx16 = static_cast<uint16_t>(i);
				for (int ty = b.minY; ty <= b.maxY; ++ty)
				{
					const int rowBase = ty * tileCols;
					for (int tx = b.minX; tx <= b.maxX; ++tx)
					{
						m_shapeTileIndices[m_shapeTileOffsets[rowBase + tx]++] = shapeIdx16;
					}
				}
			}

			return {tileCols, tileRows, INDEX_TEX_WIDTH, indexTexHeight, visibleShapes, fadeStart, fadeSpan, fadeReach};
		}

		void SDF2DRenderPipeline::renderDistanceField(const vec4* dotData, uint32_t dotCount, const vec4* shapeData,
													  const Shape2DAABB* shapeBounds, uint32_t shapeCount,
													  const Camera& camera, double time, double delta)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderDistanceField (UI)" : "renderDistanceField (World)");

			// Build and upload all CPU acceleration buffers BEFORE binding FBOs so no texture upload
			// occurs while Pass 1's framebuffer is open/unflushed (avoids Mali TBDR driver stalls).
			ShapeTileGridInfo tileGrid{};
			const bool hasShapesToDraw = (shapeCount > 0 || m_config.debugShapeTiles);
			bool drawShapesPass = false;
			if (hasShapesToDraw)
			{
				tileGrid = buildShapeTileGrid(shapeBounds, shapeCount, camera);
				drawShapesPass = (tileGrid.visibleShapes > 0 || m_config.debugShapeTiles);
				if (drawShapesPass)
				{
					// Each 2D shape instance occupies 3 vec4 slots: meta, p0, p1
					if (shapeCount > 0)
					{
						m_shapeDataBuffer.uploadData<vec4>(shapeData, static_cast<size_t>(shapeCount) * 3);
					}
					m_shapeTileHeaderBuffer.uploadRG32UI(m_shapeTileHeader.data(), tileGrid.tileCols,
														 tileGrid.tileRows);
					m_shapeTileIndicesBuffer.uploadR16UI(m_shapeTileIndices.data(), tileGrid.indexTexWidth,
														 tileGrid.indexTexHeight);
				}
			}

			const bool needGrid = (dotCount > 0) || m_config.debugGrid;
			GridInfo grid{};
			if (needGrid)
			{
				grid = buildAccelerationGrid(dotData, dotCount, camera);
				if (dotCount > 0)
				{
					m_dotDataBuffer.uploadData<vec4>(dotData, dotCount);
				}
				m_gridHeaderBuffer.uploadData2D<glm::vec4>(m_gridHeader.data(), grid.gridCols, grid.gridRows);
				m_gridIndicesBuffer.uploadData2D<glm::vec4>(m_gridIndices.data(), grid.indexTexWidth,
															grid.indexTexHeight);
			}

			// -----------------------------------------------------------------
			// Pass 1: Render Shape Distance Field (16x16 screen-space tile grid)
			// -----------------------------------------------------------------
			{
				PROFILE_SCOPE(m_config.isUI ? "renderDistanceField Shapes (UI)" : "renderDistanceField Shapes (World)");
				m_shapeRender.bind();
				invalidateColorAttachment0();

				if (!hasShapesToDraw)
				{
					// Stay within GL_RGBA16F finite range (max 65504.0) to avoid +Inf during linear sampling
					glClearColor(50000.0f, 0.0f, 0.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				else if (!drawShapesPass)
				{
					glClearColor(std::min(tileGrid.fadeReach, 50000.0f), 0.0f, 0.0f, 1.0f);
					glClear(GL_COLOR_BUFFER_BIT);
				}
				else
				{
					m_shapeShader.use();
					m_shapeShader.setUniform("u_camMatrix", camera.view);
					m_shapeShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
					m_shapeShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
					m_shapeShader.setUniform("u_time", static_cast<float>(time));
					m_shapeShader.setUniform("u_audioVolume", WeirdAudio::AudioEngine::getInstance().getAudioVolume());
					m_shapeShader.setUniform("u_shapeCount", static_cast<int>(shapeCount));
					m_shapeShader.setUniform("u_shapeTileCols", tileGrid.tileCols);
					m_shapeShader.setUniform("u_shapeTileRows", tileGrid.tileRows);
					m_shapeShader.setUniform("u_shapeFadeRange", glm::vec2(tileGrid.fadeStart, tileGrid.fadeSpan));
					m_shapeShader.setUniform("u_shapeFadeReach", tileGrid.fadeReach);

					// Keep Pass 1 usampler2D textures on dedicated units 5 & 6 (never aliased with Pass 2 sampler2D).
					m_shapeShader.setUniform("t_shapeBuffer", 0);
					m_shapeDataBuffer.bind(0);
					m_shapeShader.setUniform("t_shapeTileHeader", 5);
					m_shapeTileHeaderBuffer.bind(5);
					m_shapeShader.setUniform("t_shapeTileIndices", 6);
					m_shapeTileIndicesBuffer.bind(6);

					m_renderPlane.draw(m_shapeShader);
				}
#ifdef WEIRD_USE_FBDEV_EGL
				// Workaround for ARM Mali-G31 Bifrost (r20p0 fbdev EGL): chaining Pass 1 (sdf_shapes.frag reading
				// GL_RG32UI/GL_R16UI usampler2D tile buffers and writing m_shapeTexture) directly into Pass 2
				// (sdf_dots.frag sampling m_shapeTexture + dot buffers) without a hard barrier triggers a ~300 ms/frame
				// driver stall. Moving buffer uploads upfront and calling glFlush() do not clear it; glFinish() drops
				// renderDistanceField (UI) from ~300 ms down to ~8 ms.
				glFinish();
#endif
				Profiler::get().gpuSync();
			}

			// -----------------------------------------------------------------
			// Pass 2: Render Dots Distance Field & Composite (Motion Blur)
			// -----------------------------------------------------------------
			{
				PROFILE_SCOPE(m_config.isUI ? "renderDistanceField Dots (UI)" : "renderDistanceField Dots (World)");
				int previousDistanceIndex = m_distanceTextureDoubleBufferIdx;
				m_distanceTextureDoubleBufferIdx = (m_distanceTextureDoubleBufferIdx + 1) % 2;

				m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->bind();
				invalidateColorAttachment0();
				m_distanceShader.use();

				// Set uniforms
				if (m_resetCameraReprojection || m_oldCameraMatrix == glm::mat4(1.0f))
				{
					m_oldCameraMatrix = camera.view;
					m_prevFrameCameraMatrix = camera.view;
					m_lastCameraPosition = camera.position;
					cameraPositionChange = glm::vec3(0.0f);
					m_resetCameraReprojection = false;
				}
				else
				{
					m_prevFrameCameraMatrix = m_oldCameraMatrix;
					cameraPositionChange = camera.position - m_lastCameraPosition;
					m_lastCameraPosition = camera.position;
				}

				m_distanceShader.setUniform("u_camMatrix", camera.view);
				m_distanceShader.setUniform("u_oldCamMatrix", m_oldCameraMatrix);
				m_oldCameraMatrix = camera.view;

				m_distanceShader.setUniform("u_camPositionChange", cameraPositionChange);

				m_distanceShader.setUniform("u_time", static_cast<float>(time));
				m_distanceShader.setUniform("u_deltaTime", static_cast<float>(delta));
				m_distanceShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
				m_distanceShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
				m_distanceShader.setUniform("u_motionBlurBlendSpeed", m_config.motionBlurBlendSpeed);
				m_distanceShader.setUniform("u_k", m_config.ballK);

				// Slot 0: Previous frame distance texture (motion blur)
				m_distanceShader.setUniform("t_colorTexture", 0);
				m_distanceTextureDoubleBuffer[previousDistanceIndex]->getColorAttachment()->bind(0);

				// Slot 1: Intermediate Shape Distance texture (from Pass 1)
				m_distanceShader.setUniform("t_shapeDistanceTexture", 1);
				m_shapeTexture.bind(1);

				// Slot 2: Dot data buffer (positions & materials)
				m_distanceShader.setUniform("u_dotCount", static_cast<int>(dotCount));
				if (needGrid)
				{
					m_distanceShader.setUniform("t_dotBuffer", 2);
					m_dotDataBuffer.bind(2);

					// Slot 3 & 4: Acceleration grid
					m_distanceShader.setUniform("u_gridBoundsMin", glm::vec2(grid.minX, grid.minY));
					m_distanceShader.setUniform("u_gridStep", glm::vec2(grid.stepX, grid.stepY));
					m_distanceShader.setUniform("u_dotFadeRange", glm::vec2(grid.fadeStart, grid.fadeSpan));
					m_distanceShader.setUniform("u_gridCols", grid.gridCols);
					m_distanceShader.setUniform("u_gridRows", grid.gridRows);

					m_distanceShader.setUniform("t_gridHeader", 3);
					m_gridHeaderBuffer.bind(3);
					m_distanceShader.setUniform("t_gridIndices", 4);
					m_gridIndicesBuffer.bind(4);
				}

				m_renderPlane.draw(m_distanceShader);
				Profiler::get().gpuSync();

#ifdef WEIRD_TEST_HOOKS
				if (Profiler::get().isRecordingReport())
				{
					static bool s_loggedWorld = false;
					static bool s_loggedUI = false;
					bool& logged = m_config.isUI ? s_loggedUI : s_loggedWorld;
					if (!logged)
					{
						logged = true;
						std::cout << "[SDF2D Stats " << (m_config.isUI ? "UI" : "World")
								  << "] sampleRes=" << m_distanceSampleWidth << "x" << m_distanceSampleHeight
								  << " shapeCount=" << shapeCount << " visibleShapes=" << tileGrid.visibleShapes
								  << " shapeTiles=" << tileGrid.tileCols << "x" << tileGrid.tileRows
								  << " shapeIdxH=" << tileGrid.indexTexHeight << " dotCount=" << dotCount
								  << " visDots=" << grid.visibleObjects << " dotGrid=" << grid.gridCols << "x"
								  << grid.gridRows << " dotIdxH=" << grid.indexTexHeight << " ballK=" << m_config.ballK
								  << std::endl;
					}
				}
#endif
			}
		}

		void SDF2DRenderPipeline::applyJumpFloodCorrection(const Camera& camera)
		{
			PROFILE_SCOPE(m_config.isUI ? "applyJumpFloodCorrection (UI)" : "applyJumpFloodCorrection (World)");

			const float sampleW = static_cast<float>(std::max(1u, m_distanceSampleWidth));
			const float sampleH = static_cast<float>(std::max(1u, m_distanceSampleHeight));
			const float maxDim = std::max(sampleW, sampleH);
			int startJump = largestPowerOfTwoBelow(static_cast<int>(maxDim));

			// Regular shadows only march up to SHADOW_WORLD_DISTANCE / zoom in UV space (0.4 / zoom) and AO
			// saturates within ambienOcclusionRadius * 0.001, so cap startJump to the actual reach needed.
			// Long shadows and lighting-distance debug views keep the full-screen startJump.
			if (!m_config.enableLongShadows && !m_config.debugLightingDistanceField)
			{
				constexpr float SHADOW_WORLD_DISTANCE = 0.4f;
				const float absZoom = std::max(std::abs(camera.view[3].z), 1e-4f);
				const float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);
				const float shadowFarUV = std::min(1.4f, SHADOW_WORLD_DISTANCE / absZoom);
				const float shadowTexels = shadowFarUV * sampleW;
				const float aoTexels = (m_config.ambienOcclusionRadius * 0.001f / (1.0f + overscan)) * sampleH;
				const float neededReach = std::ceil(std::max(shadowTexels, aoTexels)) + 4.0f;

				// Jump sequence S, S/2, ..., 1 + JFA+1 covers Chebyshev radius 2*S texels.
				int targetJump = 4;
				while (targetJump * 2 < static_cast<int>(neededReach) && targetJump < startJump)
				{
					targetJump *= 2;
				}
				startJump = std::min(startJump, targetJump);
			}

			bool pingpong = true;

			// Initialize
			m_jumpFloodInitRender.bind();
			invalidateColorAttachment0();
			m_jumpFloodInitShader.use();

			m_jumpFloodInitShader.setUniform("u_texelSize", glm::vec2(1.0f / sampleW, 1.0f / sampleH));
			m_jumpFloodInitShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
			m_jumpFloodInitShader.setUniform("t_distanceTexture", 0);
			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_renderPlane.draw(m_jumpFloodInitShader);

			// Jump flood passes
			m_jumpFloodStepShader.use();
			m_jumpFloodStepShader.setUniform("t_prevSeeds", 0);
			m_jumpFloodStepShader.setUniform("u_aspectScale", sampleH / sampleW);
			m_jumpFloodStepShader.setUniform("u_resolution", glm::ivec2(m_distanceSampleWidth, m_distanceSampleHeight));

			int jump = startJump;
			bool first = true;

			while (jump >= 1)
			{
				m_jumpFloodDoubleBuffer[pingpong]->bind();
				invalidateColorAttachment0();

				m_jumpFloodStepShader.setUniform("u_jump", jump);

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

			// JFA+1: one final jump=1 pass tightens the Voronoi assignment near boundaries
			m_jumpFloodDoubleBuffer[pingpong]->bind();
			invalidateColorAttachment0();
			m_jumpFloodStepShader.setUniform("u_jump", 1);
			m_jumpFloodDoubleBuffer[!pingpong]->getColorAttachment()->bind(0);
			m_renderPlane.draw(m_jumpFloodStepShader);
			pingpong = !pingpong;

			// The lighting shader samples this directly, clamping negative (inside) values before the square root
			m_lastFloodTextureIdx = pingpong ? 0 : 1;
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::upscaleDistance()
		{
			PROFILE_SCOPE(m_config.isUI ? "upscaleDistance (UI)" : "upscaleDistance (World)");

			m_distanceUpscaler.bind();
			invalidateColorAttachment0();

			m_distanceUpscalerShader.use();
			m_distanceUpscalerShader.setUniform("u_originalResolution",
												vec2(m_distanceSampleWidth, m_distanceSampleHeight));
			m_distanceUpscalerShader.setUniform("u_targetResolution",
												vec2(m_config.renderWidth, m_config.renderHeight));
			m_distanceUpscalerShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
			m_distanceUpscalerShader.setUniform("t_data", 0);

			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_renderPlane.draw(m_distanceUpscalerShader);
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::renderMaterialColors(const Material2D* materials, const Camera& camera, double time,
													   double delta)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderMaterialColors (UI)" : "renderMaterialColors (World)");

			m_colorRender.bind();
			invalidateColorAttachment0();

			m_materialColorShader.use();
			m_materialColorShader.setUniform("u_camMatrix", camera.view);
			m_materialColorShader.setUniform("u_oldCamMatrix", m_prevFrameCameraMatrix);
			m_materialColorShader.setUniform("u_time", static_cast<float>(time));
			m_materialColorShader.setUniform("u_deltaTime", static_cast<float>(delta));
			m_materialColorShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));
			for (int i = 0; i < 16; i++)
			{
				std::string prefix = "u_materials[" + std::to_string(i) + "].";
				m_materialColorShader.setUniform(prefix + "color", materials[i].color);
				m_materialColorShader.setUniform(prefix + "secondaryColor", materials[i].secondaryColor);
				m_materialColorShader.setUniform(prefix + "pattern", static_cast<int>(materials[i].pattern));
				m_materialColorShader.setUniform(prefix + "patternScale", materials[i].patternScale);
			}
			m_materialColorShader.setUniform("u_materialBlendSpeed", m_config.materialBlendSpeed);
			m_materialColorShader.setUniform("u_camPositionChange", cameraPositionChange);

			m_materialColorShader.setUniform("t_materialDataTexture", 0);
			m_distanceUpscaled.bind(0);

			m_materialColorShader.setUniform("t_currentColorTexture", 1);
			m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(1);

			m_renderPlane.draw(m_materialColorShader);
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::blendMaterials(double time)
		{
			if (m_materialBlendIterations == 0)
			{
				return;
			}

			PROFILE_SCOPE(m_config.isUI ? "blendMaterials (UI)" : "blendMaterials (World)");

			m_materialBlendShader.use();
			m_materialBlendShader.setUniform("t_colorTexture", 0);
			m_materialBlendShader.setUniform("u_time", time);

			for (unsigned int i = 0; i < m_materialBlendIterations * 2; i++)
			{
				m_postProcessDoubleBuffer[horizontal]->bind();
				invalidateColorAttachment0();

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

			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::renderBackground(const Camera& camera, double time, const BackgroundParams& bgParams)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderBackground (UI)" : "renderBackground (World)");

			// Inject code if needed
			bool isDirty =
				(m_lastBackgroundType != bgParams.type) || (m_lastBackgroundCustomCode != bgParams.customShaderCode);
			if (isDirty)
			{
				m_lastBackgroundType = bgParams.type;
				m_lastBackgroundCustomCode = bgParams.customShaderCode;
				std::string injectedCode = "";

				switch (bgParams.type)
				{
					case BackgroundType::Solid:
						injectedCode = "vec3 getBackground(vec2 uv, vec2 worldPos) { return u_bgPrimaryColor.rgb; }";
						break;
					case BackgroundType::Grid:
						injectedCode = "vec3 getBackground(vec2 uv, vec2 worldPos) {\n"
									   "    float zoom = -1.0 * u_camMatrix[3].z;\n"
									   "    float freq = 0.1 * u_bgScale;\n"
									   "    float threshold = 2.0 * freq * zoom / u_resolution.y;\n"
									   "    float gridLine = (fract(freq * worldPos.x) >= threshold && \n"
									   "                      fract(freq * worldPos.y) >= threshold) ? 1.0 : 0.0;\n"
									   "    return mix(u_bgPrimaryColor.rgb, u_bgSecondaryColor.rgb, 1.0 - gridLine) * "
									   "u_bgIntensity;\n"
									   "}";
						break;
					case BackgroundType::Checkers:
						injectedCode = "vec3 getBackground(vec2 uv, vec2 worldPos) {\n"
									   "    float freq = 0.1 * u_bgScale;\n"
									   "    vec2 p = floor(freq * worldPos);\n"
									   "    float check = mod(p.x + p.y, 2.0);\n"
									   "    return mix(u_bgPrimaryColor.rgb, u_bgSecondaryColor.rgb, check) * "
									   "u_bgIntensity;\n"
									   "}";
						break;
					case BackgroundType::Sky:
						injectedCode =
							"vec3 getBackground(vec2 uv, vec2 worldPos) {\n"
							"    float freq = 0.1 * u_bgScale;\n"
							"    float t = clamp((worldPos.y * freq + 1.0) * 0.5, 0.0, 1.0);\n"
							"    return mix(u_bgSecondaryColor.rgb, u_bgPrimaryColor.rgb, t) * u_bgIntensity;\n"
							"}";
						break;
					case BackgroundType::Custom:
						injectedCode = bgParams.customShaderCode;
						break;
				}

				injectedCode = "#define HAS_CUSTOM_BACKGROUND\n" + injectedCode;
				m_defaultBackgroundShader.setFragmentIncludeCode(0, injectedCode);
			}

			int writeIdx = (m_backgroundDoubleBufferIdx + 1) % 2;
			m_backgroundDoubleBuffer[writeIdx]->bind();
			invalidateColorAttachment0();

			m_defaultBackgroundShader.use();
			m_defaultBackgroundShader.setUniform("t_prevBackground", 0);
			if (m_backgroundDoubleBufferIdx == 0)
			{
				m_backgroundTextureFront.bind(0);
			}
			else
			{
				m_backgroundTextureBack.bind(0);
			}
			m_defaultBackgroundShader.setUniform("u_camMatrix", camera.view);
			m_defaultBackgroundShader.setUniform("u_time", time);
			m_defaultBackgroundShader.setUniform("u_resolution",
												 glm::vec2(m_config.renderWidth, m_config.renderHeight));

			// Background params
			m_defaultBackgroundShader.setUniform("u_bgPrimaryColor", bgParams.primaryColor);
			m_defaultBackgroundShader.setUniform("u_bgSecondaryColor", bgParams.secondaryColor);
			m_defaultBackgroundShader.setUniform("u_bgScale", bgParams.scale);
			m_defaultBackgroundShader.setUniform("u_bgIntensity", bgParams.intensity);
			m_defaultBackgroundShader.setUniform("u_bgParallax", glm::clamp(bgParams.parallax, 0.0f, 1.0f));
			m_defaultBackgroundShader.setUniform("u_enableBlend", time <= 1.0);

			m_renderPlane.draw(m_defaultBackgroundShader);
			Profiler::get().gpuSync();
			m_backgroundDoubleBufferIdx = writeIdx;
		}

		void SDF2DRenderPipeline::applyLighting(const Material2D* materials, const std::vector<Light2D>& lights,
												const Camera& camera, double time, Texture* backgroundTexture)
		{
			PROFILE_SCOPE(m_config.isUI ? "applyLighting (UI)" : "applyLighting (World)");

			m_litSceneRender.bind();
			invalidateColorAttachment0();

			m_lightingShader.use();
			m_lightingShader.setUniform("u_camMatrix", camera.view);
			m_lightingShader.setUniform("u_time", static_cast<float>(time));
			m_lightingShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));
			m_lightingShader.setUniform("u_ambienOcclusionRadius", m_config.ambienOcclusionRadius);
			m_lightingShader.setUniform("u_ambienOcclusionStrength", m_config.ambienOcclusionStrength);
			m_lightingShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
			m_lightingShader.setUniform("u_shadowTint", m_config.shadowTint);
			m_lightingShader.setUniform("u_refractionIntensity", m_config.refractionIntensity);

			int numLights = std::min(static_cast<int>(lights.size()), 8);
			m_lightingShader.setUniform("u_numLights", numLights);
			for (int i = 0; i < numLights; i++)
			{
				std::string prefix = "u_lights[" + std::to_string(i) + "].";
				m_lightingShader.setUniform(prefix + "type", static_cast<int>(lights[i].type));
				m_lightingShader.setUniform(prefix + "position", lights[i].position);
				m_lightingShader.setUniform(prefix + "direction", lights[i].direction);
				m_lightingShader.setUniform(prefix + "color", lights[i].color);
				m_lightingShader.setUniform(prefix + "radius", lights[i].radius);
				m_lightingShader.setUniform(prefix + "coneAngle", lights[i].coneAngle);
				m_lightingShader.setUniform(prefix + "conePenumbra", lights[i].conePenumbra);
				m_lightingShader.setUniform(prefix + "castShadows", lights[i].castShadows ? 1 : 0);
			}

			for (int i = 0; i < 16; i++)
			{
				std::string prefix = "u_materials[" + std::to_string(i) + "].";
				m_lightingShader.setUniform(prefix + "color", materials[i].color);
				m_lightingShader.setUniform(prefix + "secondaryColor", materials[i].secondaryColor);
				m_lightingShader.setUniform(prefix + "pattern", static_cast<int>(materials[i].pattern));
				m_lightingShader.setUniform(prefix + "patternScale", materials[i].patternScale);
			}

			// Color texture
			m_lightingShader.setUniform("t_colorTexture", 0);

			if (m_materialBlendIterations > 0)
			{
				m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
			}
			else
			{
				m_colorTexture.bind(0);
			}

			// Distance
			m_lightingShader.setUniform("t_distanceSampledTexture", 1);
			m_distanceUpscaled.bind(1);

			// Background
			m_lightingShader.setUniform("t_backgroundTexture", 2);
			if (backgroundTexture)
			{
				backgroundTexture->bind(2);
			}
			else
			{
				if (m_backgroundDoubleBufferIdx == 0)
				{
					m_backgroundTextureFront.bind(2);
				}
				else
				{
					m_backgroundTextureBack.bind(2);
				}
			}

			// Flood field (seed + squared distance) for long shadows, or uncorrected SDF for short shadows/AO
			if (m_config.enableShadows)
			{
				m_lightingShader.setUniform("t_floodTexture", 3);
				if (m_config.enableLongShadows)
				{
					m_jumpFloodDoubleBuffer[m_lastFloodTextureIdx]->getColorAttachment()->bind(3);
				}
				else
				{
					m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(3);
				}
			}

			m_renderPlane.draw(m_lightingShader);
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::showDebugUI()
		{
#ifndef WEIRD_DISABLE_IMGUI
			const char* label = m_config.isUI ? "UI Pipeline" : "World Pipeline";
			if (!ImGui::CollapsingHeader(label))
				return;

			ImGui::PushID(label);

			ImGui::SeparatorText("Shadows");
			if (ImGui::Checkbox("Shadows", &m_config.enableShadows))
			{
				if (m_config.enableShadows)
					m_lightingShader.addDefine("SHADOWS_ENABLED");
				else
					m_lightingShader.removeDefine("SHADOWS_ENABLED");
			}
			if (ImGui::Checkbox("Long Shadows", &m_config.enableLongShadows))
			{
				if (m_config.enableLongShadows)
					m_lightingShader.addDefine("LONG_SHADOWS");
				else
					m_lightingShader.removeDefine("LONG_SHADOWS");
			}

			ImGui::SeparatorText("Rendering");
			if (ImGui::Checkbox("Antialiasing", &m_config.enableAntialiasing))
			{
				if (m_config.enableAntialiasing)
					m_lightingShader.addDefine("ANTIALIASING");
				else
					m_lightingShader.removeDefine("ANTIALIASING");
			}
			if (ImGui::Checkbox("Motion Blur", &m_config.enableMotionBlur))
			{
				if (m_config.enableMotionBlur)
				{
					m_distanceShader.addDefine("MOTION_BLUR");
					if (m_config.motionBlurMethod == MotionBlurMethod::FillOverride)
						m_distanceShader.addDefine("MOTION_BLUR_FILL_OVERRIDE");
				}
				else
				{
					m_distanceShader.removeDefine("MOTION_BLUR");
					m_distanceShader.removeDefine("MOTION_BLUR_FILL_OVERRIDE");
				}
			}
			if (m_config.enableMotionBlur)
			{
				ImGui::SliderFloat("Motion Blur Speed", &m_config.motionBlurBlendSpeed, 0.001f, 20.0f);
				int methodIdx = (m_config.motionBlurMethod == MotionBlurMethod::FillOverride) ? 1 : 0;
				const char* methods[] = {"Asymmetric Delta", "Fill Override"};
				if (ImGui::Combo("Motion Blur Method", &methodIdx, methods, 2))
				{
					m_config.motionBlurMethod =
						(methodIdx == 1) ? MotionBlurMethod::FillOverride : MotionBlurMethod::AsymmetricDelta;
					if (m_config.motionBlurMethod == MotionBlurMethod::FillOverride)
						m_distanceShader.addDefine("MOTION_BLUR_FILL_OVERRIDE");
					else
						m_distanceShader.removeDefine("MOTION_BLUR_FILL_OVERRIDE");
				}
			}
			if (ImGui::Checkbox("Refraction", &m_config.enableRefraction))
			{
				if (m_config.enableRefraction)
					m_lightingShader.addDefine("REFRACTION");
				else
					m_lightingShader.removeDefine("REFRACTION");
			}

			ImGui::SeparatorText("Debug");
			if (ImGui::Checkbox("Show Distance Field", &m_config.debugDistanceField))
			{
				if (m_config.debugDistanceField)
					m_lightingShader.addDefine("DEBUG_SHOW_DISTANCE");
				else
					m_lightingShader.removeDefine("DEBUG_SHOW_DISTANCE");
			}

			if (ImGui::Checkbox("Show Lighting Distance Field", &m_config.debugLightingDistanceField))
			{
				if (m_config.debugLightingDistanceField)
					m_lightingShader.addDefine("DEBUG_SHOW_LIGHTING_DISTANCE");
				else
					m_lightingShader.removeDefine("DEBUG_SHOW_LIGHTING_DISTANCE");
			}

			if (ImGui::Checkbox("Show Material Colors", &m_config.debugMaterialColors))
			{
				if (m_config.debugMaterialColors)
					m_lightingShader.addDefine("DEBUG_SHOW_COLORS");
				else
					m_lightingShader.removeDefine("DEBUG_SHOW_COLORS");
			}

			if (ImGui::Checkbox("Show grid", &m_config.debugGrid))
			{
				if (m_config.debugGrid)
					m_distanceShader.addDefine("DEBUG_SHOW_GRID");
				else
					m_distanceShader.removeDefine("DEBUG_SHOW_GRID");
			}

			if (ImGui::Checkbox("Show shape tiles", &m_config.debugShapeTiles))
			{
				if (m_config.debugShapeTiles)
					m_shapeShader.addDefine("DEBUG_SHOW_SHAPE_TILES");
				else
					m_shapeShader.removeDefine("DEBUG_SHOW_SHAPE_TILES");
			}

			ImGui::SeparatorText("Ambient Occlusion");
			ImGui::SliderFloat("AO Radius", &m_config.ambienOcclusionRadius, 0.0f, 20.0f);
			ImGui::SliderFloat("AO Strength", &m_config.ambienOcclusionStrength, 0.0f, 1.0f);

			ImGui::SeparatorText("Materials");
			ImGui::SliderFloat("Blend Speed", &m_config.materialBlendSpeed, 0.0f, 20.0f);
			ImGui::SliderFloat("Smooth Factor (k)", &m_config.ballK, 0.0f, 50.0f);

			ImGui::PopID();
#endif
		}

		Shader& SDF2DRenderPipeline::getShapeShader()
		{
			return m_shapeShader;
		}

		Shader& SDF2DRenderPipeline::getDistanceShader()
		{
			return m_distanceShader;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
