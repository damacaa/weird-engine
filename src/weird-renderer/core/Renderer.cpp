#include "weird-renderer/core/Renderer.h"

#include <ctime>
#include <filesystem>
#include <sys/stat.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>

#include "weird-engine/Profiler.h"
#include "weird-engine/Logger.h"
#include "weird-renderer/core/MeshRenderPipeline.h"

#ifndef SHADERS_PATH
#define SHADERS_PATH
#endif // !SHADERS_PATH

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		Renderer::Renderer(const DisplaySettings& settings, SDL_Window*& window)
			: m_window(window)
			, m_distanceSampleScale(settings.distanceSampleScale)
			, m_renderScale(settings.internalResolutionScale)
			, m_vSyncEnabled(settings.vSyncEnabled)
			, m_ditheringSpread(std::max(0.0f, settings.ditheringSpread))
			, m_ditheringColorCount(std::max(2, settings.ditheringColorCount))
			, m_uiPipeline(nullptr)
			, m_worldPipeline(nullptr)
			, m_3DWorldPipeline(nullptr)
			, m_meshPipeline(nullptr)
			, m_uiCamera((vec3(0.0f, 0.0f, 0.0f)))
			, m_targetRefreshRate(settings.refreshRate)
		{
			std::copy_n(settings.colorPalette, 16, m_colorPalette);
			setWindowSize(settings.width, settings.height);

			// Initialize world 2D pipeline
			SDF2DRenderPipeline::Config worldConfig;
			worldConfig.renderWidth = m_renderWidth;
			worldConfig.renderHeight = m_renderHeight;
			worldConfig.distanceSampleScale = m_distanceSampleScale;
			worldConfig.distanceOverscan = settings.worldDistanceOverscan;
			worldConfig.renderScale = m_renderScale;
			worldConfig.isUI = false;
			worldConfig.enableShadows = true;
			worldConfig.enableLongShadows = settings.enableLongShadows;
			worldConfig.shadowTint = settings.shadowTint;
			worldConfig.enableRefraction = true;
			worldConfig.enableAntialiasing = (m_renderScale >= 1.0f);
			worldConfig.enableMotionBlur = true;
			worldConfig.materialBlendIterations = 1;
			worldConfig.materialBlendSpeed = 10.0f;
			worldConfig.motionBlurBlendSpeed = 10.0f;
			worldConfig.debugDistanceField = false;
			worldConfig.debugMaterialColors = false;
			worldConfig.ambienOcclusionRadius = 5.0f;
			worldConfig.ambienOcclusionStrength = 0.2f;
			worldConfig.ballK = settings.worldSmoothFactor;
			m_worldPipeline = new SDF2DRenderPipeline(worldConfig, m_colorPalette, m_renderPlane);

			// Initialize UI 2D pipeline
			SDF2DRenderPipeline::Config uiConfig;
			uiConfig.renderWidth = m_renderWidth;
			uiConfig.renderHeight = m_renderHeight;
			uiConfig.distanceSampleScale = m_distanceSampleScale;
			uiConfig.distanceOverscan = 0.0f;
			uiConfig.renderScale = m_renderScale;
			uiConfig.isUI = true;
			uiConfig.enableShadows = false;
			uiConfig.enableLongShadows = false;
			uiConfig.enableRefraction = true;
			uiConfig.enableAntialiasing = true;
			uiConfig.enableMotionBlur = true;
			uiConfig.materialBlendIterations = 1;
			uiConfig.materialBlendSpeed = 5.0f;
			uiConfig.motionBlurBlendSpeed = 5.0f;
			uiConfig.debugDistanceField = false;
			uiConfig.debugMaterialColors = false;
			uiConfig.ballK = settings.uiSmoothFactor;
			uiConfig.ambienOcclusionRadius = 7.0f;
			uiConfig.ambienOcclusionStrength = 0.15f;
			m_uiPipeline = new SDF2DRenderPipeline(uiConfig, m_colorPalette, m_renderPlane);

			// Initialize 3D SDF pipeline
			SDF3DRenderPipeline::Config sdf3DConfig;
			sdf3DConfig.renderWidth = m_renderWidth;
			sdf3DConfig.renderHeight = m_renderHeight;
			sdf3DConfig.contrast = settings.raymarching3DContrast;
			sdf3DConfig.enablePathTracer = settings.enable3DPathTracer;
			m_3DWorldPipeline =
				new SDF3DRenderPipeline(sdf3DConfig, m_renderPlane);

			// Initialize mesh pipeline
			m_meshPipeline = new MeshRenderPipeline();
			m_meshPipeline->resize(m_renderWidth, m_renderHeight);

			m_combineScenesShaderProgram =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "postprocess/alpha_blend_textures.frag");

			m_outputShaderProgram =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "postprocess/screen_output.frag");

			m_ditheringEnabled = settings.enableDithering;
			if (m_ditheringEnabled)
				m_outputShaderProgram.addDefine("DITHERING");

			m_surfaceBlurEnabled    = settings.enableSurfaceBlur;
			m_surfaceBlurRadius     = std::max(1.0f, settings.surfaceBlurRadius);
			m_surfaceBlurSigmaColor = std::max(0.001f, settings.surfaceBlurSigmaColor);
			if (m_surfaceBlurEnabled)
				m_outputShaderProgram.addDefine("SURFACE_BLUR");

			// Enable culling
			glCullFace(GL_BACK);
			glFrontFace(GL_CCW);
		}

		Renderer::~Renderer()
		{
			freeAll();
			delete m_worldPipeline;
			delete m_uiPipeline;
			delete m_3DWorldPipeline;
			delete m_meshPipeline;
			m_combineScenesShaderProgram.free();
			m_outputShaderProgram.free();
		}

		void Renderer::render(Scene& scene, const double time, const double delta)
		{
			PROFILE_SCOPE("Scene render");
			double clampedDelta = std::clamp(delta, 0.0, 1.0 / m_targetRefreshRate);
			clampedDelta = scene.getTime() > 1.0 ? clampedDelta : scene.getTime() * clampedDelta;

			auto& result = renderScene(scene, time, clampedDelta);
			output(scene, result, clampedDelta);
			glFinish();

			{
				PROFILE_SCOPE("ImGui");
				static bool showDebugUI = false;
				static bool showStatsUI = false;

				if (Input::GetKeyDown(Input::F3))
				{
					showDebugUI = !showDebugUI;
				}

				if (Input::GetKeyDown(Input::F4))
				{
					showStatsUI = !showStatsUI;
					if (showStatsUI)
						Profiler::Get().enableRealtime();
					else
						Profiler::Get().disableRealtime();
				}

				if (Input::GetKeyDown(Input::F11))
				{
					bool isFullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
					SDL_SetWindowFullscreen(m_window, !isFullscreen);
				}

				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();

				if (showDebugUI)
				{
					ImGui::Begin("Engine Settings");

					if (ImGui::BeginTabBar("EngineTabBar"))
					{
						if (ImGui::BeginTabItem("Scene"))
						{
							scene.renderImGui();
							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem("Renderer"))
						{
							m_worldPipeline->showDebugUI();
							m_uiPipeline->showDebugUI();
							m_3DWorldPipeline->showDebugUI();
							m_meshPipeline->showDebugUI();

							// Output settings
							{
								const char* label = "Output Settings";
								if (ImGui::CollapsingHeader(label))
								{
									ImGui::PushID(label);

									if (ImGui::Checkbox("Enable Dithering", &m_ditheringEnabled))
									{
										if (m_ditheringEnabled)
											m_outputShaderProgram.addDefine("DITHERING");
										else
											m_outputShaderProgram.removeDefine("DITHERING");
									}

									ImGui::SliderFloat("Dithering Spread", &m_ditheringSpread, 0.0f, 1.0f);
									ImGui::SliderInt("Dithering Color Count", &m_ditheringColorCount, 2, 32);

									ImGui::Separator();
									if (ImGui::Checkbox("Enable Surface Blur", &m_surfaceBlurEnabled))
									{
										if (m_surfaceBlurEnabled)
											m_outputShaderProgram.addDefine("SURFACE_BLUR");
										else
											m_outputShaderProgram.removeDefine("SURFACE_BLUR");
									}
									ImGui::SliderFloat("Surface Blur Radius", &m_surfaceBlurRadius, 1.0f, 12.0f);
									ImGui::SliderFloat("Surface Blur Edge Threshold", &m_surfaceBlurSigmaColor, 0.01f,
													   1.0f);

									ImGui::PopID();
								}
							}

							if (ImGui::Button("Take Screenshot"))
							{
								m_takeScreenshot = true;
							}

							bool isFullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN) != 0;
							if (ImGui::Checkbox("Fullscreen", &isFullscreen))
							{
								SDL_SetWindowFullscreen(m_window, isFullscreen);
							}

							if (!m_lastScreenshotPath.empty())
							{
								ImGui::SameLine();
								ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
								ImGui::TextUnformatted(m_lastScreenshotPath.c_str());
								ImGui::PopStyleColor();
								if (ImGui::IsItemHovered())
								{
									ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
									ImGui::SetTooltip("Click to open");
								}
								if (ImGui::IsItemClicked())
								{
									std::string url = "file://" + m_lastScreenshotPath;
									SDL_OpenURL(url.c_str());
								}
							}
							ImGui::EndTabItem();
						}

						if (ImGui::BeginTabItem("Console"))
						{
							ImGui::Checkbox("Print to std::cout", &Logger::s_enableConsoleOutput);
							Logger::drawImGuiConsole();
							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}

					ImGui::End();
				}

				if (showStatsUI)
				{
					drawStatsUI(scene, delta);
				}

				ImGui::Render();
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glViewport(0, 0, m_windowWidth, m_windowHeight);
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			}

			{
				PROFILE_SCOPE("Synchronization");
				SDL_GL_SwapWindow(m_window);
			}
		}

		void Renderer::setWindowTitle(const char* name)
		{
			if (m_window)
			{
				SDL_SetWindowTitle(m_window, name);
			}
		}

		void Renderer::setWindowSize(unsigned int width, unsigned int height)
		{
			m_windowWidth = width;
			m_windowHeight = height;
			m_renderWidth = width * m_renderScale;
			m_renderHeight = height * m_renderScale;

			Display::width = m_windowWidth;
			Display::height = m_windowHeight;
			Display::rWidth = m_renderWidth;
			Display::rHeight = m_renderHeight;

			freeAll();

			m_combineResultTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_combinationRender = RenderTarget(false);
			m_combinationRender.bindColorTextureToFrameBuffer(m_combineResultTexture);

			m_outputTexture = Texture(m_windowWidth, m_windowHeight, Texture::TextureType::Data);
			m_outputResolutionRender = RenderTarget(false);
			m_outputResolutionRender.bindColorTextureToFrameBuffer(m_outputTexture);

			m_renderPlane = RenderPlane();

			// Resize pipelines if they've been initialized
			if (m_worldPipeline)
			{
				m_worldPipeline->resize(m_renderWidth, m_renderHeight);
				m_uiPipeline->resize(m_renderWidth, m_renderHeight);
				m_3DWorldPipeline->resize(m_renderWidth, m_renderHeight);
				m_meshPipeline->resize(m_renderWidth, m_renderHeight);
			}

			glm::vec3 position = glm::vec3(0.0f, 0.0f, (float)m_renderHeight);
			glm::vec3 orientation = glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
			auto cameraMatrix = glm::lookAt(position, position + orientation, up);

			m_uiCamera.view = cameraMatrix;

			if (m_vSyncEnabled)
			{
				// Try -1 (Adaptive) first
				int result = SDL_GL_SetSwapInterval(-1);

				// If Adaptive fails, fall back to Standard (1)
				if (result != 0)
				{
					result = SDL_GL_SetSwapInterval(1);
				}
			}
			else
			{
				SDL_GL_SetSwapInterval(0); // Disable VSync
			}
		}

		void Renderer::output(Scene& scene, Texture& texture, const double delta)
		{

			PROFILE_SCOPE("UI Render");

			// Render UI
			// Shape data
			m_uiPipeline->getDistanceShader().use();
			scene.updateUIShader(m_uiPipeline->getDistanceShader());

			static uint32_t dataSize;
			static uint32_t shapeCount;
			static vec4* uiData = nullptr;
			scene.getUIData(uiData, dataSize, shapeCount);

			double time = scene.getTime();
			auto& m_finalResultTexture =
				m_uiPipeline->render(uiData, dataSize, shapeCount, m_uiCamera, time, delta, &texture);

			// TODO: abstract this
			glDisable(GL_DEPTH_TEST);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_windowWidth, m_windowHeight);

			m_outputShaderProgram.use();
			m_outputShaderProgram.setUniform("u_time", scene.getTime());
			m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
			m_outputShaderProgram.setUniform("u_renderResolution", glm::vec2(m_renderWidth, m_renderHeight));
			m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);
			m_outputShaderProgram.setUniform("u_ditheringSpread", m_ditheringSpread);
			m_outputShaderProgram.setUniform("u_ditheringColorCount", m_ditheringColorCount);
			m_outputShaderProgram.setUniform("u_surfaceBlurRadius", m_surfaceBlurRadius);
			m_outputShaderProgram.setUniform("u_surfaceBlurSigmaColor", m_surfaceBlurSigmaColor);

			m_outputShaderProgram.setUniform("t_colorTexture", 0);
			m_finalResultTexture.bind(0);

			m_renderPlane.draw(m_outputShaderProgram);

			// Screenshot
			if (m_takeScreenshot)
			{
				m_takeScreenshot = false;
				std::time_t t = std::time(nullptr);
				char timeBuf[32];
				std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", std::localtime(&t));
				std::string filename = std::string("screenshot_") + timeBuf + ".bmp";
				m_outputResolutionRender.bind();
				m_renderPlane.draw(m_outputShaderProgram);
				m_outputTexture.saveToDisk(filename.c_str());
				m_lastScreenshotPath = std::filesystem::absolute(filename).string();
			}
		}

		void Renderer::freeAll()
		{
			m_combinationRender.free();
			m_outputResolutionRender.free();

			m_combineResultTexture.dispose();
			m_outputTexture.dispose();
		}

		SDL_Window* Renderer::getWindow()
		{
			return m_window;
		}

		void Renderer::drawStatsUI(Scene& scene, double delta)
		{
			float fps = delta > 0.0 ? (float)(1.0 / delta) : 0.0f;
			float frameTimeMs = (float)(delta * 1000.0);

			m_frametimeHistory[m_historyOffset] = frameTimeMs;
			m_historyOffset = (m_historyOffset + 1) % STATS_HISTORY_SIZE;

			ImGui::SetNextWindowSize(ImVec2(500, 640), ImGuiCond_FirstUseEver);
			ImGui::Begin("Performance Stats");

			// FPS / Frametime header
			ImGui::Text("FPS: %.1f  |  Frame: %.1f ms", fps, frameTimeMs);

			char ftOverlay[32];
			snprintf(ftOverlay, sizeof(ftOverlay), "%.1f ms", frameTimeMs);
			ImGui::PlotLines("##ft", m_frametimeHistory, STATS_HISTORY_SIZE, m_historyOffset,
							 ftOverlay, 0.0f, 100.0f, ImVec2(ImGui::GetContentRegionAvail().x, 50));

			ImGui::Spacing();

			ImGui::TextDisabled("Profiler Scopes  (last frame)");
			ImGui::Spacing();

			auto& profiler = Profiler::Get();
			const auto& stats = profiler.getLastFrameStats();

			if (stats.empty())
			{
				ImGui::TextDisabled("Warming up...");
				ImGui::End();
				return;
			}

			// Top-level scope time as the 100% baseline
			double topMs = 0.0;
			for (const auto& s : stats)
			{
				if (s.depth == 0 && s.count > 0)
				{
					topMs = s.totalTimeMs / s.count;
					break;
				}
			}

			if (topMs <= 0.0)
				topMs = 1.0;

			static const ImVec4 depthColors[] = {
				{0.30f, 0.70f, 1.00f, 1.0f},  // depth 0 — blue
				{0.35f, 0.90f, 0.50f, 1.0f},  // depth 1 — green
				{1.00f, 0.70f, 0.25f, 1.0f},  // depth 2 — orange
				{0.85f, 0.40f, 0.90f, 1.0f},  // depth 3 — purple
				{0.85f, 0.35f, 0.35f, 1.0f},  // depth 4 — red
			};
			constexpr int MAX_DEPTH_COLORS = 5;
			const float NAME_COLUMN_W = 180.0f;
			const float INDENT_PX = 16.0f;
			const float ROW_H = ImGui::GetTextLineHeight() + 2.0f;

			if (ImGui::BeginTabBar("ProfilerTabs"))
			{
				if (ImGui::BeginTabItem("List View"))
				{
					for (size_t i = 0; i < stats.size(); ++i)
					{
						const auto& stat = stats[i];
						if (stat.count == 0 || stat.depth == 0)
							continue;

						double avgMs = stat.totalTimeMs / stat.count;

						float fraction = (float)(avgMs / topMs);
						fraction = std::min(1.0f, std::max(0.0f, fraction));
						float indentOff = (stat.depth - 1) * INDENT_PX;
						float availW = ImGui::GetContentRegionAvail().x;

						// Scope name (indented)
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentOff);
						ImGui::TextUnformatted(stat.name);

						// Progress bar on the same line
						ImGui::SameLine(NAME_COLUMN_W + indentOff + 10.0f);
						char barLabel[64];
						snprintf(barLabel, sizeof(barLabel), "%.3f ms  %.3f%%", avgMs, fraction * 100.0f);
						float barW = availW - NAME_COLUMN_W - indentOff - 10.0f;
						if (barW > 10.0f)
						{
							int ci = std::min(stat.depth - 1, MAX_DEPTH_COLORS - 1);
							ImGui::PushStyleColor(ImGuiCol_PlotHistogram, depthColors[ci]);
							ImGui::ProgressBar(fraction, ImVec2(barW, ROW_H), barLabel);
							ImGui::PopStyleColor();
						}
					}
					ImGui::EndTabItem();
				}

				if (ImGui::BeginTabItem("Timeline View"))
				{
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					ImVec2 p0 = ImGui::GetCursorScreenPos();
					float availW = ImGui::GetContentRegionAvail().x;
					float rowH = ImGui::GetTextLineHeight() + 4.0f;

					int maxDepth = 0;
					for (const auto& s : stats)
					{
						if (s.depth > 0)
							maxDepth = std::max(maxDepth, s.depth - 1);
					}

					float totalH = (maxDepth + 1) * rowH;
					ImGui::InvisibleButton("##timeline", ImVec2(availW, totalH));

					float currentX[32] = {0};

					for (size_t i = 0; i < stats.size(); ++i)
					{
						const auto& stat = stats[i];
						if (stat.count == 0 || stat.depth == 0 || stat.depth >= 33)
							continue;

						int renderDepth = stat.depth - 1;
						double avgMs = stat.totalTimeMs / stat.count;
						float fraction = (float)(avgMs / topMs);
						fraction = std::min(1.0f, std::max(0.0f, fraction));
						float w = std::max(1.0f, fraction * availW);

						float x = currentX[renderDepth];
						float y = renderDepth * rowH;

						ImVec2 rectMin = ImVec2(p0.x + x, p0.y + y);
						ImVec2 rectMax = ImVec2(p0.x + x + w, p0.y + y + rowH - 1.0f);

						int ci = std::min(renderDepth, MAX_DEPTH_COLORS - 1);
						ImU32 col = ImGui::ColorConvertFloat4ToU32(depthColors[ci]);

						bool hovered = ImGui::IsMouseHoveringRect(rectMin, rectMax);
						if (hovered)
						{
							ImVec4 hc = depthColors[ci];
							hc.x = std::min(1.0f, hc.x * 1.2f);
							hc.y = std::min(1.0f, hc.y * 1.2f);
							hc.z = std::min(1.0f, hc.z * 1.2f);
							col = ImGui::ColorConvertFloat4ToU32(hc);
						}

						drawList->AddRectFilled(rectMin, rectMax, col);
						drawList->AddRect(rectMin, rectMax, IM_COL32(0, 0, 0, 100));

						if (w > 20.0f)
						{
							ImGui::PushClipRect(rectMin, rectMax, true);
							drawList->AddText(ImVec2(rectMin.x + 2, rectMin.y + 1), IM_COL32(255, 255, 255, 255), stat.name);
							ImGui::PopClipRect();
						}

						if (hovered)
						{
							ImGui::BeginTooltip();
							ImGui::Text("%s", stat.name);
							ImGui::Text("%.3f ms (%.1f%%)", avgMs, fraction * 100.0f);
							ImGui::EndTooltip();
						}

						currentX[renderDepth] += w;
						if (renderDepth + 1 < 32)
							currentX[renderDepth + 1] = x;
					}
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextDisabled("Physics Stats");

			scene.renderPhysicsStatsUI();


			if (profiler.isRealtime())
			{
				if (ImGui::Button("Start Average Report"))
				{
					profiler.startRecording();
				}
			}
			else if (profiler.isRecordingReport())
			{
				ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Recording average report...");
				if (ImGui::Button("Cancel & Return to Realtime"))
				{
					profiler.enableRealtime();
				}
			}
			else if (profiler.isReportFinished())
			{
				ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Average Report Completed");
				if (ImGui::Button("Restart Average Report"))
				{
					profiler.startRecording();
				}
				ImGui::SameLine();
				if (ImGui::Button("Return to Realtime"))
				{
					profiler.enableRealtime();
				}
			}

			ImGui::End();
		}

		Texture& Renderer::renderScene(Scene& scene, const double time, const double delta)
		{
			PROFILE_SCOPE("World Render");

			// Determine render mode
			auto renderMode = scene.getRenderMode();
			bool enable2D =
				renderMode == Scene::RenderMode::RayMarching2D || renderMode == Scene::RenderMode::RayMarchingBoth;
			bool enable3D = renderMode != Scene::RenderMode::RayMarching2D;

			// Get camera
			auto& sceneCamera = scene.getCamera();

			if(renderMode == Scene::RenderMode::RayMarchingBoth)
			{
				sceneCamera.fov = 90.0f;
			}

			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.updateMatrix(m_windowWidth, m_windowHeight);

			// 3D
			if (enable3D)
			{
				if(enable2D)
					PROFILE_SCOPE("3D Render");

				auto& lights = scene.getLigths();

				// --- 1. GBuffer pass: render mesh geometry first ---
				// Depth testing and culling must be enabled for correct GBuffer writes.
				{
					PROFILE_SCOPE("Mesh GBuffer");
					glEnable(GL_CULL_FACE);
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(GL_LEQUAL);
					glDepthMask(GL_TRUE);
					glDisable(GL_BLEND);
					glFrontFace(GL_CCW);

					// outputTarget (SDF render target) is forwarded to Scene::onRender callbacks
					m_meshPipeline->render(scene, m_3DWorldPipeline->getRenderTarget(), sceneCamera, lights);

					glFinish();
				}

				// --- 2. SDF ray-marching pass: composites with GBuffer ---
				// The SDF shader reads mesh albedo/position/normal from the GBuffer and applies
				// SDF-sourced lighting to mesh surfaces.  SDFs cast shadows/colour onto meshes.
				{
					PROFILE_SCOPE("3D SDF");
					glDisable(GL_CULL_FACE);
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(GL_ALWAYS);
					glDepthMask(GL_TRUE);

					scene.update3DWorldShader(m_3DWorldPipeline->getShader());

					static uint32_t dataSize3D = 0;
					static uint32_t shapeCount3D = 0;
					static vec4* data3D = nullptr;
					scene.get3DShapesData(data3D, dataSize3D, shapeCount3D);

					m_3DWorldPipeline->render(
						data3D, dataSize3D, shapeCount3D, lights, sceneCamera, scene.getTime(),
						m_meshPipeline->getGBufferAlbedo(),
						m_meshPipeline->getGBufferWorldPos(),
						m_meshPipeline->getGBufferNormal(),
						m_meshPipeline->getGBufferMaterial(),
						m_meshPipeline->getDepthTexture(),
						m_meshPipeline->getBackDepthTexture(),
						scene.getMaterials()
					);

					glEnable(GL_CULL_FACE);
					glEnable(GL_DEPTH_TEST);
					glDepthFunc(GL_LEQUAL);
					scene.renderExtra(m_3DWorldPipeline->getRenderTarget());
					glDisable(GL_CULL_FACE);
					glDisable(GL_DEPTH_TEST);

					glFinish();
				}

				if (!enable2D)
				{
					return m_3DWorldPipeline->getOutputTexture();
				}
			}

			// TODO: abstract this
			glDisable(GL_DEPTH_TEST);

			// 2D
			static uint32_t dataSize;
			static uint32_t shapeCount;
			static vec4* data = nullptr;
			if (enable2D)
			{
				if(enable3D)
					PROFILE_SCOPE("2D Render");

				m_worldPipeline->getDistanceShader().use();
				scene.update2DWorldShader(m_worldPipeline->getDistanceShader());
				scene.get2DShapesData(data, dataSize, shapeCount);

				auto& t = m_worldPipeline->render(data, dataSize, shapeCount, sceneCamera, scene.getTime(), delta,
												  enable3D ? &m_3DWorldPipeline->getOutputTexture() : nullptr);
				// In both pure 2D and RayMarchingBoth modes, the 2D pipeline's output is the final result.
				// When enable3D is true, the 3D texture was already passed as the background so it's baked in.
				return t;
			}

			// Pure 3D: should have been returned earlier; fall back to 3D output.
			return m_3DWorldPipeline->getOutputTexture();
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine