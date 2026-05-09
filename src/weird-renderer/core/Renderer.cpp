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
#include "weird-renderer/core/MeshRenderPipeline.h"

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
			std::copy_n(settings.materialDataPalette, 16, m_materialDataPalette);

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
			m_3DWorldPipeline =
				new SDF3DRenderPipeline(sdf3DConfig, m_colorPalette, m_materialDataPalette, m_renderPlane);

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

			// Enable culling
			glCullFace(GL_FRONT);
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

			static bool showDebugUI = false;

			if (Input::GetKeyDown(Input::F3))
			{
				showDebugUI = !showDebugUI;
			}

			if (showDebugUI)
			{
				ImGui_ImplOpenGL3_NewFrame();
				ImGui_ImplSDL3_NewFrame();
				ImGui::NewFrame();

				ImGui::Begin("Renderer Settings");
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

						ImGui::PopID();
					}
				}

				if (ImGui::Button("Take Screenshot"))
				{
					m_takeScreenshot = true;
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

				ImGui::End();

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

		Texture& Renderer::renderScene(Scene& scene, const double time, const double delta)
		{
			PROFILE_SCOPE("World Render");

			const float NEAR_PLANE = 0.1f;
			const float FAR_PLANE = 300.0f;

			// Get camera
			auto& sceneCamera = scene.getCamera();
			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.updateMatrix(NEAR_PLANE, FAR_PLANE, m_windowWidth, m_windowHeight);

			// Determine render mode
			auto renderMode = scene.getRenderMode();
			bool enable2D =
				renderMode == Scene::RenderMode::RayMarching2D || renderMode == Scene::RenderMode::RayMarchingBoth;
			bool enable3D = renderMode != Scene::RenderMode::RayMarching2D;

			// 3D
			if (enable3D)
			{
				PROFILE_SCOPE("3D Render");

				auto& lights = scene.getLigths();

				// Clear the 3D output target
				m_3DWorldPipeline->getRenderTarget().bind();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glClearColor(0, 0, 0, 1);
				glClearDepthf(1.0f);

				// Enable culling and depth testing for 3D meshes
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LEQUAL);
				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);

				// SDF ray marching pass
				// TODO: render meshes before ray marching to reduce overdraw
				{
					PROFILE_SCOPE("3D SDF");
					scene.update3DWorldShader(m_3DWorldPipeline->getShader());

					static uint32_t dataSize3D = 0;
					static uint32_t shapeCount3D = 0;
					static vec4* data3D = nullptr;
					scene.get3DShapesData(data3D, dataSize3D, shapeCount3D);

					m_3DWorldPipeline->render(data3D, dataSize3D, shapeCount3D, lights, sceneCamera, scene.getTime(),
											  m_meshPipeline->getDepthTexture());

					glFinish();
				}

				// Mesh geometry pass (on top of ray marching result)
				{
					PROFILE_SCOPE("Render 3D Models");
					m_meshPipeline->render(scene, m_3DWorldPipeline->getRenderTarget(), sceneCamera, lights);
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
			Texture* lit2DSceneTexture = nullptr;
			static uint32_t dataSize;
			static uint32_t shapeCount;
			static vec4* data = nullptr;
			if (enable2D)
			{
				m_worldPipeline->getDistanceShader().use();
				scene.update2DWorldShader(m_worldPipeline->getDistanceShader());
				scene.get2DShapesData(data, dataSize, shapeCount);

				auto& t = m_worldPipeline->render(data, dataSize, shapeCount, sceneCamera, scene.getTime(), delta,
												  enable3D ? &m_3DWorldPipeline->getOutputTexture() : nullptr);
				lit2DSceneTexture = &t;

				if (!enable3D)
				{
					return t;
				}
			}

			m_combinationRender.bind();
			m_combineScenesShaderProgram.use();
			m_combineScenesShaderProgram.setUniform("u_time", scene.getTime());
			m_combineScenesShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));
			m_combineScenesShaderProgram.setUniform("t_3DSceneTexture", 0);
			m_3DWorldPipeline->getOutputTexture().bind(0);
			m_combineScenesShaderProgram.setUniform("t_2DSceneTexture", 1);
			lit2DSceneTexture->bind(1);

			m_renderPlane.draw(m_combineScenesShaderProgram);

			return m_combineResultTexture;
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine