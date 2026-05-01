#include "weird-renderer/core/SDF2DRenderPipeline.h"

#include <algorithm>

#include <imgui.h>

#include "weird-engine/vec.h"
#include "weird-engine/Profiler.h"

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

			m_backgroundTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundRender = RenderTarget(false);
			m_backgroundRender.bindColorTextureToFrameBuffer(m_backgroundTexture);

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
			m_backgroundTexture.dispose();
			m_litSceneTexture.dispose();

			// Recreate textures with new dimensions and rebind to existing render targets
			m_distanceTextureA =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderA.bindColorTextureToFrameBuffer(m_distanceTextureA);

			m_distanceTextureB =
				Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_distanceRenderB.bindColorTextureToFrameBuffer(m_distanceTextureB);

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

			m_backgroundTexture = Texture(m_config.renderWidth, m_config.renderHeight, Texture::TextureType::Color);
			m_backgroundRender.bindColorTextureToFrameBuffer(m_backgroundTexture);

			m_litSceneTexture =
				Texture(m_config.renderWidth, m_config.renderHeight,
						m_config.renderScale <= 0.5f ? Texture::TextureType::RetroColor : Texture::TextureType::Data);
			m_litSceneRender.bindColorTextureToFrameBuffer(m_litSceneTexture);
		}

		Texture& SDF2DRenderPipeline::render(vec4* shapeData, uint32_t dataSize, uint32_t shapeCount,
											 const Camera& camera, double time, double delta,
											 Texture* backgroundTexture)
		{
			// Execute all pipeline stages
			renderDistanceField(shapeData, dataSize, shapeCount, camera, time, delta);

			applyJumpFloodCorrection(time); // To generate a corrected distance texture

			upscaleDistance();
			renderMaterialColors(camera, time, delta);
			blendMaterials(time);
			renderBackground(camera, time);
			applyLighting(camera, time, backgroundTexture);

			glFinish(); // Makes sense for profiler

			return m_litSceneTexture;
		}

		SDF2DRenderPipeline::GridInfo SDF2DRenderPipeline::buildAccelerationGrid(vec4* shapeData, uint32_t dataSize,
																				 uint32_t shapeCount,
																				 const Camera& camera)
		{
			PROFILE_SCOPE(m_config.isUI ? "Build grid (UI)" : "Build grid (World)");

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

			float idealCellSize = cellPad;
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
				// +1/-1: one extra cell of buffer so the SDF never drops close to 0 at the outer
				// insertion boundary, preventing JFA from propagating a "ghost" surface into empty cells.
				int minCellX = static_cast<int>((cx - cellPad - minX) / stepX) - 1;
				int minCellY = static_cast<int>((cy - cellPad - minY) / stepY) - 1;
				int maxCellX = static_cast<int>((cx + cellPad - minX) / stepX) + 1;
				int maxCellY = static_cast<int>((cy + cellPad - minY) / stepY) + 1;

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
			{
				PROFILE_SCOPE(m_config.isUI ? "Upload data (UI)" : "Upload data (World)");

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
				PROFILE_SCOPE(m_config.isUI ? "Render distance (UI)" : "Render distance (World)");
				m_renderPlane.draw(m_distanceShader);
				glFinish(); // Makes sense for profiler
			}
		}

		void SDF2DRenderPipeline::applyJumpFloodCorrection(double time)
		{
			float maxDim = std::max<float>(m_distanceSampleWidth, m_distanceSampleHeight);
			uint16_t jumpFloodIterations = largestPowerOfTwoBelow(maxDim);
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
		}

		void SDF2DRenderPipeline::upscaleDistance()
		{
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
		}

		void SDF2DRenderPipeline::renderMaterialColors(const Camera& camera, double time, double delta)
		{
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
			m_defaultBackgroundShader.setUniform("u_resolution",
												 glm::vec2(m_config.renderWidth, m_config.renderHeight));

			m_renderPlane.draw(m_defaultBackgroundShader);
		}

		void SDF2DRenderPipeline::applyLighting(const Camera& camera, double time, Texture* backgroundTexture)
		{
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
				m_backgroundTexture.bind(2);
			}

			// Corrected distance for shadows
			m_lightingShader.setUniform("t_distanceCorrectedTexture", 3);
			m_distanceTextureCorrected.bind(3);

			m_renderPlane.draw(m_lightingShader);
		}

		void SDF2DRenderPipeline::handleDebugInputs()
		{
			const char* label = m_config.isUI ? "UI Pipeline" : "World Pipeline";
			if (!ImGui::CollapsingHeader(label))
				return;

			ImGui::PushID(label);

			ImGui::SeparatorText("Shadows");
			if (ImGui::Checkbox("Shadows", &m_config.enableShadows))
			{
				if (m_config.enableShadows) m_lightingShader.addDefine("SHADOWS_ENABLED");
				else m_lightingShader.removeDefine("SHADOWS_ENABLED");
			}
			if (ImGui::Checkbox("Long Shadows", &m_config.enableLongShadows))
			{
				if (m_config.enableLongShadows) m_lightingShader.addDefine("LONG_SHADOWS");
				else m_lightingShader.removeDefine("LONG_SHADOWS");
			}

			ImGui::SeparatorText("Rendering");
			if (ImGui::Checkbox("Antialiasing", &m_config.enableAntialiasing))
			{
				if (m_config.enableAntialiasing) m_lightingShader.addDefine("ANTIALIASING");
				else m_lightingShader.removeDefine("ANTIALIASING");
			}
			if (ImGui::Checkbox("Motion Blur", &m_config.enableMotionBlur))
			{
				if (m_config.enableMotionBlur) m_distanceShader.addDefine("MOTION_BLUR");
				else m_distanceShader.removeDefine("MOTION_BLUR");
			}
			if (ImGui::Checkbox("Refraction", &m_config.enableRefraction))
			{
				if (m_config.enableRefraction) m_lightingShader.addDefine("REFRACTION");
				else m_lightingShader.removeDefine("REFRACTION");
			}

			ImGui::SeparatorText("Debug");
			if (ImGui::Checkbox("Show Distance Field", &m_config.debugDistanceField))
			{
				if (m_config.debugDistanceField) m_lightingShader.addDefine("DEBUG_SHOW_DISTANCE");
				else m_lightingShader.removeDefine("DEBUG_SHOW_DISTANCE");
			}
			if (ImGui::Checkbox("Show Material Colors", &m_config.debugMaterialColors))
			{
				if (m_config.debugMaterialColors) m_lightingShader.addDefine("DEBUG_SHOW_COLORS");
				else m_lightingShader.removeDefine("DEBUG_SHOW_COLORS");
			}

			ImGui::SeparatorText("Ambient Occlusion");
			ImGui::SliderFloat("AO Radius", &m_config.ambienOcclusionRadius, 0.0f, 20.0f);
			ImGui::SliderFloat("AO Strength", &m_config.ambienOcclusionStrength, 0.0f, 1.0f);

			ImGui::SeparatorText("Materials");
			ImGui::SliderFloat("Blend Speed", &m_config.materialBlendSpeed, 0.0f, 20.0f);
			ImGui::SliderFloat("Smooth Factor (k)", &m_config.ballK, 0.0f, 50.0f);

			ImGui::PopID();
		}

		Shader& SDF2DRenderPipeline::getDistanceShader()
		{
			return m_distanceShader;
		}
	} // namespace WeirdRenderer
} // namespace WeirdEngine
