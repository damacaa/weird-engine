#include "weird-renderer/core/SDF2DRenderPipeline.h"

#include <algorithm>

#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

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

		int SDF2DRenderPipeline::largestPowerOfTwoBelow(int n)
		{
			return ::WeirdEngine::WeirdRenderer::largestPowerOfTwoBelow(n);
		}

		SDF2DRenderPipeline::SDF2DRenderPipeline(const Config& config, const glm::vec4* colorPalette,
												 RenderPlane& renderPlane)
			: m_config(config)
			, m_colorPalette(colorPalette)
			, m_renderPlane(renderPlane)
			, m_distanceSampleWidth(0)
			, m_distanceSampleHeight(0)
			, m_materialBlendIterations(config.materialBlendIterations)
			, m_oldCameraMatrix(1.0f)
			, m_prevFrameCameraMatrix(1.0f)
			, m_lastCameraPosition(0.0f)
		{
			float overscan = std::clamp(m_config.distanceOverscan, 0.0f, 0.5f);
			float overscanScale = 1.0f + 2.0f * overscan;
			m_distanceSampleWidth = (unsigned int)(m_config.renderWidth * m_config.distanceSampleScale * overscanScale);
			m_distanceSampleHeight =
				(unsigned int)(m_config.renderHeight * m_config.distanceSampleScale * overscanScale);

			// Load shaders
			m_distanceShader = Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/sdf_distance.frag");

			if (config.ballK > 0.0f)
				m_distanceShader.addDefine("BLEND_SHAPES");

			if (m_config.enableMotionBlur)
			{
				m_distanceShader.addDefine("MOTION_BLUR");
			}

			if (m_config.isUI)
			{
				m_distanceShader.addDefine("UI_PIPELINE");
			}

			m_jumpFloodInitShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/jump_flood_init.frag");
			m_jumpFloodStepShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/jump_flood_step.frag");
			m_distanceCorrectionShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/distance_correction.frag");
			m_distanceUpscalerShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/distance_upscaler.frag");
			m_materialColorShader =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "2d/material_color.frag");
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

			m_distanceTextureCorrected =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_distanceCorrectionRender = RenderTarget(false);
			m_distanceCorrectionRender.bindColorTextureToFrameBuffer(m_distanceTextureCorrected);

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

			m_backgroundRenderFront.free();
			m_backgroundRenderBack.free();

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
			m_backgroundTextureFront.dispose();
			m_backgroundTextureBack.dispose();
			m_litSceneTexture.dispose();

			// Recreate textures with new dimensions and rebind to existing render targets
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

			m_distanceTextureCorrected =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_distanceCorrectionRender.bindColorTextureToFrameBuffer(m_distanceTextureCorrected);

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

		Texture& SDF2DRenderPipeline::render(vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
											 const Camera& camera, double time, double delta,
											 const BackgroundParams& bgParams, Texture* backgroundTexture)
		{
			// PROFILE_SCOPE(m_config.isUI ? "SDF2DRenderPipeline (UI)" : "SDF2DRenderPipeline (World)");

			// Execute all pipeline stages
			renderDistanceField(shapeData, dataSize, shapeCount, camera, time, delta);

			applyJumpFloodCorrection(time); // To generate a corrected distance texture

			upscaleDistance();
			renderMaterialColors(camera, time, delta);
			blendMaterials(time);
			if (!backgroundTexture)
			{
				renderBackground(camera, time, bgParams);
			}

			// Dynamically set shadow tint based on background primary color to simulate global sky ambient
			glm::vec3 hsvAmbient = glm::hsvColor(glm::vec3(bgParams.primaryColor));

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

			applyLighting(camera, time, backgroundTexture);

			Profiler::get().gpuSync(); // Stalls if recording average report

			return m_litSceneTexture;
		}

		SDF2DRenderPipeline::GridInfo SDF2DRenderPipeline::buildAccelerationGrid(vec4* shapeData, uint32_t dataSize,
																				 uint32_t shapeCount,
																				 const Camera& camera)
		{
			// Build acceleration grid using CPU padding spatial partitioning.
			// Grid bounds are derived entirely from the camera frustum, so off-screen circles are
			// never inserted. This relies on the 2D camera convention where:
			//   zoom  = -view[3].z   (positive distance from origin)
			//   camXY = view[3].xy   (camera translation in world space)
			// If the camera representation changes, update the frustum derivation below accordingly.
			constexpr int indexTexWidth = 1024;

			int objectCount = static_cast<int>(dataSize) - (2 * static_cast<int>(shapeCount));

			float objectRadius = m_config.isUI ? 5.0f : 1.0f;
			float cellPad = m_config.ballK + objectRadius;

			// Derive the visible world-space rectangle from the camera matrix
			float zoom = -camera.view[3].z;
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

			// Expand frustum by cellPad so circles that straddle the view edge are included
			float minX = viewMinX - cellPad;
			float minY = viewMinY - cellPad;
			float maxX = viewMaxX + cellPad;
			float maxY = viewMaxY + cellPad;

			// Cell size is the object diameter so a circle fits in ~1 cell (2x2 worst-case).
			// ballK only affects the insertion radius, not the cell size.
			float idealCellSize = 2.0f * objectRadius;
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
				float cx = shapeData[i].x;
				float cy = shapeData[i].y;
				int minCellX = static_cast<int>((cx - cellPad - minX) / stepX);
				int minCellY = static_cast<int>((cy - cellPad - minY) / stepY);
				int maxCellX = static_cast<int>((cx + cellPad - minX) / stepX);
				int maxCellY = static_cast<int>((cy + cellPad - minY) / stepY);

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

			// Pre-allocate index buffer at final padded size to avoid realloc during fill
			int indexTexHeight = std::max(1, (totalIndices + indexTexWidth - 1) / indexTexWidth);
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

			return {minX, minY, stepX, stepY, gridCols, gridRows, indexTexWidth, indexTexHeight};
		}

		void SDF2DRenderPipeline::renderDistanceField(vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
													  const Camera& camera, double time, double delta)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderDistanceField (UI)" : "renderDistanceField (World)");

			{
				// PROFILE_SCOPE(m_config.isUI ? "Upload data (UI)" : "Upload data (World)");

				int previousDistanceIndex = m_distanceTextureDoubleBufferIdx;
				m_distanceTextureDoubleBufferIdx = (m_distanceTextureDoubleBufferIdx + 1) % 2;

				GridInfo grid = buildAccelerationGrid(shapeData, dataSize, shapeCount, camera);
				m_gridHeaderBuffer.uploadData2D<glm::vec4>(m_gridHeader.data(), grid.gridCols, grid.gridRows);
				m_gridIndicesBuffer.uploadData2D<glm::vec4>(m_gridIndices.data(), grid.indexTexWidth,
															grid.indexTexHeight);

				m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->bind();
				m_distanceShader.use();

				// Set uniforms
				m_prevFrameCameraMatrix = m_oldCameraMatrix; // save before overwrite so other shaders can access the
															 // true previous frame matrix
				if (m_oldCameraMatrix == glm::mat4(1.0f))
				{
					m_oldCameraMatrix = camera.view;
					m_prevFrameCameraMatrix = camera.view;
				}

				m_distanceShader.setUniform("u_camMatrix", camera.view);
				m_distanceShader.setUniform("u_oldCamMatrix", m_oldCameraMatrix);
				m_oldCameraMatrix = camera.view;

				cameraPositionChange = camera.position - m_lastCameraPosition;
				m_lastCameraPosition = camera.position;
				m_distanceShader.setUniform("u_camPositionChange", cameraPositionChange);

				m_distanceShader.setUniform("u_time", time);
				m_distanceShader.setUniform("u_deltaTime", static_cast<float>(delta));
				m_distanceShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
				m_distanceShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
				m_distanceShader.setUniform("u_motionBlurBlendSpeed", m_config.motionBlurBlendSpeed);
				m_distanceShader.setUniform("u_k", m_config.ballK);
				m_distanceShader.setUniform("u_staticColors", m_colorPalette, 16);

				m_distanceShader.setUniform("t_colorTexture", 0);
				m_distanceTextureDoubleBuffer[previousDistanceIndex]->getColorAttachment()->bind(0);

				m_distanceShader.setUniform("u_loadedObjects", (int)dataSize);
				m_distanceShader.setUniform("u_customShapeCount", static_cast<int>(shapeCount));
				m_shapeDataBuffer.uploadData<vec4>(shapeData, dataSize);
				m_distanceShader.setUniform("t_shapeBuffer", 1);
				m_shapeDataBuffer.bind(1);

				m_distanceShader.setUniform("u_gridBoundsMin", glm::vec2(grid.minX, grid.minY));
				m_distanceShader.setUniform("u_gridStep", glm::vec2(grid.stepX, grid.stepY));
				m_distanceShader.setUniform("u_gridCols", grid.gridCols);
				m_distanceShader.setUniform("u_gridRows", grid.gridRows);

				m_distanceShader.setUniform("t_gridHeader", 2);
				m_gridHeaderBuffer.bind(2);
				m_distanceShader.setUniform("t_gridIndices", 3);
				m_gridIndicesBuffer.bind(3);
			}

			// Upload grid
			{
				// PROFILE_SCOPE(m_config.isUI ? "Render distance (UI)" : "Render distance (World)");
				m_renderPlane.draw(m_distanceShader);
				Profiler::get().gpuSync();
			}
		}

		void SDF2DRenderPipeline::applyJumpFloodCorrection(double time)
		{
			PROFILE_SCOPE(m_config.isUI ? "applyJumpFloodCorrection (UI)" : "applyJumpFloodCorrection (World)");

			float maxDim =
				std::max(static_cast<float>(m_distanceSampleWidth), static_cast<float>(m_distanceSampleHeight));
			uint16_t jumpFloodIterations = static_cast<uint16_t>(largestPowerOfTwoBelow(static_cast<int>(maxDim)));
			bool pingpong = true;

			// Initialize
			m_jumpFloodInitRender.bind();
			m_jumpFloodInitShader.use();

			m_jumpFloodInitShader.setUniform("u_texelSize",
											 glm::vec2(1.0f / m_distanceSampleWidth, 1.0f / m_distanceSampleHeight));
			m_jumpFloodInitShader.setUniform("t_distanceTexture", 0);
			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_renderPlane.draw(m_jumpFloodInitShader);

			// Jump flood passes
			m_jumpFloodStepShader.use();
			m_jumpFloodStepShader.setUniform("t_prevSeeds", 0);
			m_jumpFloodStepShader.setUniform("u_texelSize",
											 glm::vec2(1.0f / m_distanceSampleWidth, 1.0f / m_distanceSampleHeight));

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
			m_distanceCorrectionShader.setUniform("u_resolution",
												  glm::vec2(m_distanceSampleWidth, m_distanceSampleHeight));
			m_distanceCorrectionShader.setUniform("u_time", time);
			m_distanceCorrectionShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));

			m_distanceCorrectionShader.setUniform("t_originalDistanceTexture", 0);
			m_distanceTextureDoubleBuffer[m_distanceTextureDoubleBufferIdx]->getColorAttachment()->bind(0);

			m_distanceCorrectionShader.setUniform("t_distanceTexture", 1);
			int lastIndex = pingpong ? 0 : 1;
			m_jumpFloodDoubleBuffer[lastIndex]->getColorAttachment()->bind(1);

			m_renderPlane.draw(m_distanceCorrectionShader);
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::upscaleDistance()
		{
			PROFILE_SCOPE(m_config.isUI ? "upscaleDistance (UI)" : "upscaleDistance (World)");

			m_distanceUpscaler.bind();

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

		void SDF2DRenderPipeline::renderMaterialColors(const Camera& camera, double time, double delta)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderMaterialColors (UI)" : "renderMaterialColors (World)");

			m_colorRender.bind();

			m_materialColorShader.use();
			m_materialColorShader.setUniform("u_camMatrix", camera.view);
			m_materialColorShader.setUniform("u_oldCamMatrix", m_prevFrameCameraMatrix);
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
			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::blendMaterials(double time)
		{
			PROFILE_SCOPE(m_config.isUI ? "blendMaterials (UI)" : "blendMaterials (World)");

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

			Profiler::get().gpuSync();
		}

		void SDF2DRenderPipeline::renderBackground(const Camera& camera, double time, const BackgroundParams& bgParams)
		{
			PROFILE_SCOPE(m_config.isUI ? "renderBackground (UI)" : "renderBackground (World)");

			// Inject code if needed
			if (bgParams.isDirty)
			{
				std::string injectedCode = "";

				switch (bgParams.type)
				{
					case BackgroundType::Solid:
						injectedCode = "vec3 getBackground(vec2 uv, vec2 worldPos) { return u_bgPrimaryColor.rgb; }";
						break;
					case BackgroundType::Grid:
						injectedCode = "vec3 getBackground(vec2 uv, vec2 worldPos) {\n"
									   "    float zoom = -2.0 * u_camMatrix[3].z;\n"
									   "    float freq = 0.1 * u_bgScale;\n"
									   "    float threshold = 2.0 * freq * zoom / u_resolution.y;\n"
									   "    float gridLine = (fract(freq * worldPos.x) >= threshold && \n"
									   "                      fract(freq * worldPos.y) >= threshold) ? 1.0 : 0.0;\n"
									   "    return mix(u_bgPrimaryColor.rgb, u_bgSecondaryColor.rgb, 1.0 - gridLine) * "
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
			m_defaultBackgroundShader.setUniform("u_enableBlend", time <= 1.0);

			m_renderPlane.draw(m_defaultBackgroundShader);
			Profiler::get().gpuSync();
			m_backgroundDoubleBufferIdx = writeIdx;
		}

		void SDF2DRenderPipeline::applyLighting(const Camera& camera, double time, Texture* backgroundTexture)
		{
			PROFILE_SCOPE(m_config.isUI ? "applyLighting (UI)" : "applyLighting (World)");

			m_litSceneRender.bind();

			m_lightingShader.use();
			m_lightingShader.setUniform("u_camMatrix", camera.view);
			m_lightingShader.setUniform("u_time", time);
			m_lightingShader.setUniform("u_resolution", glm::vec2(m_config.renderWidth, m_config.renderHeight));
			m_lightingShader.setUniform("u_ambienOcclusionRadius", m_config.ambienOcclusionRadius);
			m_lightingShader.setUniform("u_ambienOcclusionStrength", m_config.ambienOcclusionStrength);
			m_lightingShader.setUniform("u_overscan", std::clamp(m_config.distanceOverscan, 0.0f, 0.5f));
			m_lightingShader.setUniform("u_shadowTint", m_config.shadowTint);

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

			// Corrected distance for shadows
			m_lightingShader.setUniform("t_distanceCorrectedTexture", 3);
			m_distanceTextureCorrected.bind(3);

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
					m_distanceShader.addDefine("MOTION_BLUR");
				else
					m_distanceShader.removeDefine("MOTION_BLUR");
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

			ImGui::SeparatorText("Ambient Occlusion");
			ImGui::SliderFloat("AO Radius", &m_config.ambienOcclusionRadius, 0.0f, 20.0f);
			ImGui::SliderFloat("AO Strength", &m_config.ambienOcclusionStrength, 0.0f, 1.0f);

			ImGui::SeparatorText("Materials");
			ImGui::SliderFloat("Blend Speed", &m_config.materialBlendSpeed, 0.0f, 20.0f);
			ImGui::SliderFloat("Smooth Factor (k)", &m_config.ballK, 0.0f, 50.0f);

			ImGui::PopID();
#endif
		}

		Shader& SDF2DRenderPipeline::getDistanceShader()
		{
			return m_distanceShader;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
