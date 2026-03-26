#include "weird-renderer/core/Renderer.h"

#include "weird-engine/Profiler.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <sys/stat.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		Renderer::Renderer(const DisplaySettings& settings, SDL_Window*& window)
			: m_window(window)
			, m_distanceSampleScale(settings.distanceSampleScale)
			, m_renderScale(settings.internalResolutionScale)
			, m_vSyncEnabled(settings.vSyncEnabled)
			, m_uiPipeline(nullptr)
			, m_worldPipeline(nullptr)
			, m_uiCamera((vec3(0.0f, 0.0f, 0.0f)))
			, m_targetRefreshRate(settings.refreshRate)
		{
			std::copy_n(settings.colorPalette, 16, m_colorPalette);

			setWindowSize(settings.width, settings.height);

			// Load shaders
			m_geometryShaderProgram = Shader(SHADERS_PATH "common/geometry.vert", SHADERS_PATH "common/geometry.frag");

			m_instancedGeometryShaderProgram =
				Shader(SHADERS_PATH "common/geometry_instanced.vert", SHADERS_PATH "common/geometry.frag");

			m_3DsdfShaderProgram =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "3d/sdf_raymarching.frag");

			m_combineScenesShaderProgram =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "postprocess/alpha_blend_textures.frag");

			m_outputShaderProgram =
				Shader(SHADERS_PATH "common/screen_plane.vert", SHADERS_PATH "postprocess/screen_output.frag");

			m_3DShapeDataBuffer = new DataBuffer();

			// Enable culling
			glCullFace(GL_FRONT);
			glFrontFace(GL_CCW);
		}

		Renderer::~Renderer()
		{
			freeAll();
			// Delete all the other objects we've created
			m_geometryShaderProgram.free();
			m_instancedGeometryShaderProgram.free();
			m_3DsdfShaderProgram.free();
			m_combineScenesShaderProgram.free();
			m_outputShaderProgram.free();
			delete m_3DShapeDataBuffer;
		}

		void Renderer::render(Scene& scene, const double time, const double delta)
		{
			PROFILE_SCOPE("Scene render");
			double clampedDelta = std::clamp(delta, 0.0, 1.0 / m_targetRefreshRate);
			clampedDelta = scene.getTime() > 1.0 ? clampedDelta : scene.getTime() * clampedDelta;

			auto& result = renderScene(scene, time, clampedDelta);
			output(scene, result, clampedDelta);

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

			// Initialize 3D rendering resources
			m_geometryTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_geometryDepthTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Depth);
			m_geometryRender = RenderTarget(false);
			m_geometryRender.bindColorTextureToFrameBuffer(m_geometryTexture);
			m_geometryRender.bindDepthTextureToFrameBuffer(m_geometryDepthTexture);

			m_3DSceneTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_3DDepthSceneTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Depth);
			m_3DSceneRender = RenderTarget(false);
			m_3DSceneRender.bindColorTextureToFrameBuffer(m_3DSceneTexture);
			m_3DSceneRender.bindDepthTextureToFrameBuffer(m_3DDepthSceneTexture);

			m_combineResultTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_combinationRender = RenderTarget(false);
			m_combinationRender.bindColorTextureToFrameBuffer(m_combineResultTexture);

			m_outputResolutionRender = RenderTarget(false);

			m_renderPlane = RenderPlane();

			// Initialize world 2D pipeline
			SDF2DRenderPipeline::Config worldConfig;
			worldConfig.renderWidth = m_renderWidth;
			worldConfig.renderHeight = m_renderHeight;
			worldConfig.distanceSampleScale = m_distanceSampleScale;
			worldConfig.renderScale = m_renderScale;
			worldConfig.dataMode = SDF2DRenderPipeline::DataMode::WORLD;
			worldConfig.originAtBottomLeft = false;
			worldConfig.enableShadows = true;
			worldConfig.enableRefraction = true;
			worldConfig.enableAntialiasing = (m_renderScale >= 1.0f);
			worldConfig.enableMotionBlur = true;
			worldConfig.enableDithering = false;
			worldConfig.materialBlendIterations = 1;
			worldConfig.materialBlendSpeed = 10.0f;
			worldConfig.debugDistanceField = false;
			worldConfig.debugMaterialColors = false;
			worldConfig.ambienOcclusionRadius = 5.0f;
			worldConfig.ambienOcclusionStrength = 0.2f;
			worldConfig.ballK = 0.5f;
			m_worldPipeline = new SDF2DRenderPipeline(worldConfig, m_colorPalette, m_renderPlane);

			// Initialize UI 2D pipeline
			SDF2DRenderPipeline::Config uiConfig;
			uiConfig.renderWidth = m_renderWidth;
			uiConfig.renderHeight = m_renderHeight;
			uiConfig.distanceSampleScale = m_distanceSampleScale;
			uiConfig.renderScale = m_renderScale;
			uiConfig.dataMode = SDF2DRenderPipeline::DataMode::UI;
			uiConfig.originAtBottomLeft = true;
			uiConfig.enableShadows = false;
			uiConfig.enableRefraction = true;
			uiConfig.enableAntialiasing = true;
			uiConfig.enableMotionBlur = true;
			uiConfig.enableDithering = true;
			uiConfig.materialBlendIterations = 1;
			uiConfig.materialBlendSpeed = 5.0f;
			uiConfig.debugDistanceField = false;
			uiConfig.debugMaterialColors = false;
			uiConfig.ballK = 3.0f;
			uiConfig.ambienOcclusionRadius = 7.0f;
			uiConfig.ambienOcclusionStrength = 0.15f;
			m_uiPipeline = new SDF2DRenderPipeline(uiConfig, m_colorPalette, m_renderPlane);

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
			static WeirdRenderer::Dot2D* uiData = nullptr;
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
			m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

			m_outputShaderProgram.setUniform("t_colorTexture", 0);
			m_finalResultTexture.bind(0);

			m_renderPlane.draw(m_outputShaderProgram);

			// Screenshot
			if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKeyDown(Input::S))
			{
				m_finalResultTexture.saveToDisk("output_texture.png");
			}
		}

		void Renderer::freeAll()
		{
			if (!m_worldPipeline)
				return;

			// Delete pipelines
			delete m_worldPipeline;
			delete m_uiPipeline;

			m_geometryRender.free();
			m_3DSceneRender.free();
			m_combinationRender.free();
			m_outputResolutionRender.free();

			m_geometryTexture.dispose();
			m_geometryDepthTexture.dispose();
			m_3DSceneTexture.dispose();
			m_combineResultTexture.dispose();
		}

		SDL_Window* Renderer::getWindow()
		{
			return m_window;
		}

		Texture& Renderer::renderScene(Scene& scene, const double time, const double delta)
		{
			PROFILE_SCOPE("World Render");

			// Get camera
			auto& sceneCamera = scene.getCamera();
			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.updateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);

			// Determine render mode
			auto renderMode = scene.getRenderMode();
			bool enable2D =
				renderMode == Scene::RenderMode::RayMarching2D || renderMode == Scene::RenderMode::RayMarchingBoth;
			bool enable3D = renderMode != Scene::RenderMode::RayMarching2D;

			bool enable3DSDFs = true;

			// Render geometry
			if (enable3D)
			{
				auto& renderQueue = scene.getDrawQueue(); // TODO: sort and then draw it

				// Set up framebuffer for 3D scene rendering
				m_3DSceneRender.bind();
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear color and depth buffer
				glClearColor(0, 0, 0, 1);
				glDepthMask(GL_TRUE); // Still write to depth buffer for the 3D meshes
				glClearDepth(1.0f);	  // Make sure depth buffer is initialized correctly

				// Enable culling and depth testing for 3D meshes
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS); // Ensure depth comparison is 'less than'
				glDepthMask(GL_TRUE); // Write to depth buffer
				glDisable(GL_BLEND);

				// Render ray marching
				// TODO: render meshes before ray marching to reduce overdraw
				if (enable3DSDFs)
				{
					// Draw ray marching stuff
					m_3DsdfShaderProgram.use();

					// Set uniforms and other ray marching settings
					float shaderFov = 1.0f / tan(sceneCamera.fov * 0.5f * 0.01745329f); // PI / 180
					m_3DsdfShaderProgram.setUniform("u_camMatrix", sceneCamera.view);
					m_3DsdfShaderProgram.setUniform("u_fov", shaderFov);
					m_3DsdfShaderProgram.setUniform("u_time", scene.getTime());
					m_3DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					auto& lights = scene.getLigths();
					m_3DsdfShaderProgram.setUniform("u_lightPos", lights[0].position);
					m_3DsdfShaderProgram.setUniform("u_lightDirection", lights[0].rotation);
					m_3DsdfShaderProgram.setUniform("u_lightColor", lights[0].color);

					// Geom color and depth textures for ray marching
					m_3DsdfShaderProgram.setUniform("t_colorTexture", 0);
					m_geometryTexture.bind(0);

					m_3DsdfShaderProgram.setUniform("t_depthTexture", 1);
					m_geometryDepthTexture.bind(1);

					// Upload and bind shapes for ray marching
					static uint32_t DataSize3D = 0;
					static WeirdRenderer::Dot2D* Data3D = nullptr;
					// scene.get2DShapesData(Data3D, DataSize3D);
					m_3DsdfShaderProgram.setUniform("t_shapeBuffer", 2);
					m_3DShapeDataBuffer->uploadData<Dot2D>(Data3D, DataSize3D);
					m_3DShapeDataBuffer->bind(2);

					m_3DsdfShaderProgram.setUniform("u_loadedObjects", (int)DataSize3D);

					// Draw the render plane with ray marching shader
					m_renderPlane.draw(m_3DsdfShaderProgram);

					// Unbind textures and buffers
					m_geometryTexture.unbind();
					m_geometryDepthTexture.unbind();
					m_3DShapeDataBuffer->unbind();
				}

				// Render 3D geometry objects (with depth writing)
				{
					PROFILE_SCOPE("Render 3D Models");
					m_geometryShaderProgram.use();
					m_geometryShaderProgram.setUniform("u_camMatrix", sceneCamera.cameraMatrix);
					m_geometryShaderProgram.setUniform("u_camPos", sceneCamera.position);

					auto& lights = scene.getLigths();
					m_geometryShaderProgram.setUniform("u_lightPos", lights[0].position);
					m_geometryShaderProgram.setUniform("u_lightDirection", lights[0].rotation);
					m_geometryShaderProgram.setUniform("u_lightColor", lights[0].color);

					// Draw objects in the scene (3D models)
					scene.renderModels(m_3DSceneRender, m_geometryShaderProgram, m_instancedGeometryShaderProgram);
				}

				if (!enable2D)
				{
					return m_3DSceneTexture;
				}
			}

			// TODO: abstract this
			glDisable(GL_DEPTH_TEST);

			// 2D
			Texture* lit2DSceneTexture = nullptr;
			static uint32_t dataSize;
			static uint32_t shapeCount;
			static WeirdRenderer::Dot2D* data = nullptr;
			if (enable2D)
			{
				m_worldPipeline->getDistanceShader().use();
				scene.updateRayMarchingShader(m_worldPipeline->getDistanceShader());
				scene.get2DShapesData(data, dataSize, shapeCount);

				auto& t = m_worldPipeline->render(data, dataSize, shapeCount, sceneCamera, scene.getTime(), delta,
												  enable3D ? &m_3DSceneTexture : nullptr);
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
			m_3DSceneTexture.bind(0);
			m_combineScenesShaderProgram.setUniform("t_2DSceneTexture", 1);
			lit2DSceneTexture->bind(1);

			m_renderPlane.draw(m_combineScenesShaderProgram);

			return m_combineResultTexture;
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine