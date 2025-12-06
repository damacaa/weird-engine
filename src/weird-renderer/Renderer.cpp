#include "weird-renderer/Renderer.h"
#include "weird-renderer/Debug.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_hints.h>
#include <sys/stat.h>

// SETTINGS
static bool USE_CORRECTED_DISTANCE_TEXTURE = true;

namespace WeirdEngine {
	namespace WeirdRenderer {



		static int largestPowerOfTwoBelow(int n) {
			int p = 1;
			while (p * 2 <= n) {
				p *= 2;
			}
			return p;
		}



		Renderer::Renderer(const unsigned int width, const unsigned int height)
			: m_audioEngine()
			, m_sdlInitializer(width, height, m_window, m_audioEngine)
			, m_windowWidth(width)
			, m_windowHeight(height)
			, m_distanceSampleScale(0.5f)
			, m_distanceSampleWidth(width * m_distanceSampleScale)
			, m_distanceSampleHeight(height * m_distanceSampleScale)
			, m_renderScale(1.0f)
			, m_renderWidth(width * m_renderScale)
			, m_renderHeight(height * m_renderScale)
			, m_vSyncEnabled(true)
			, m_materialBlendIterations(1.0f / m_distanceSampleScale)
		{
			Screen::width = m_windowWidth;
			Screen::height = m_windowHeight;
			Screen::rWidth = m_renderWidth;
			Screen::rHeight = m_renderHeight;

			m_audioEngine.loadSound(SHADERS_PATH "sample.wav");

			// Load shaders
			m_geometryShaderProgram = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "default.frag");

			m_instancedGeometryShaderProgram = Shader(SHADERS_PATH "default_instancing.vert", SHADERS_PATH "default.frag");

			m_3DsdfShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "raymarching.frag");

			m_2DDistanceShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DSDFDistanceShader.frag");
			m_2DDistanceShader.addDefine("BLEND_SHAPES");
			m_2DDistanceShader.addDefine("MOTION_BLUR");

			m_2DDistanceUpscalerShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DDistanceUpscaler.frag");

			m_JumpFloodInitShader= Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "InitJumpFlooding.frag");
			m_JumpFloodStepShader= Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "JumpFlooding.frag");
			m_2DDistanceCorrectionShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DDistanceCorrection.frag");

			m_2DMaterialColorShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialColorShader.frag");

			m_2DMaterialBlendShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialBlendShader.frag");

			m_2DLightingShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DLightingShader.frag");
			m_2DLightingShader.addDefine("SHADOWS_ENABLED");
			m_2DLightingShader.addDefine("DITHERING");
			if (m_renderScale >= 1.0f)
				m_2DLightingShader.addDefine("ANTIALIASING");

			m_2DGridShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DBackground.frag");

			m_postProcessingShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "PostProcessShader.frag");

			m_combineScenesShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "combineScenes.frag");

			m_outputShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "output.frag");

			GL_CHECK_ERROR();

			// Bind textures to render planes fbo outputs
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


			m_distanceTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_2DSceneRender = RenderTarget(false);
			m_2DSceneRender.bindColorTextureToFrameBuffer(m_distanceTexture);

			m_jumpFloodInitTexture = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::Data);
			m_jumpFloodInitRender = RenderTarget(false);
			m_jumpFloodInitRender.bindColorTextureToFrameBuffer(m_jumpFloodInitTexture);

			m_JumpFloodTexturePing = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_JumpFloodTexturePong = Texture(m_distanceSampleWidth, m_distanceSampleHeight, Texture::TextureType::LinearData);
			m_JumpFloodRenderPing = RenderTarget(false);
			m_JumpFloodRenderPing.bindColorTextureToFrameBuffer(m_JumpFloodTexturePing);
			m_JumpFloodRenderPong = RenderTarget(false);
			m_JumpFloodRenderPong.bindColorTextureToFrameBuffer(m_JumpFloodTexturePong);
			m_JumpFloodDoubleBuffer[0] = &m_JumpFloodRenderPing;
			m_JumpFloodDoubleBuffer[1] = &m_JumpFloodRenderPong;

			m_distanceTextureCorrected = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_2DDistanceCorrectionRender = RenderTarget(false);
			m_2DDistanceCorrectionRender.bindColorTextureToFrameBuffer(m_distanceTextureCorrected);

			m_2DDistanceUpscaled = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_2DDistanceUpscaler = RenderTarget(false);
			m_2DDistanceUpscaler.bindColorTextureToFrameBuffer(m_2DDistanceUpscaled);

			m_2dColorTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_2DColorRender = RenderTarget(false);
			m_2DColorRender.bindColorTextureToFrameBuffer(m_2dColorTexture);

			m_postProcessTextureFront = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_postProcessRenderFront = RenderTarget(false);
			m_postProcessRenderFront.bindColorTextureToFrameBuffer(m_postProcessTextureFront);

			m_postProcessTextureBack = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_postProcessRenderBack = RenderTarget(false);
			m_postProcessRenderBack.bindColorTextureToFrameBuffer(m_postProcessTextureBack);

			m_postProcessDoubleBuffer[0] = &m_postProcessRenderFront;
			m_postProcessDoubleBuffer[1] = &m_postProcessRenderBack;

			m_lit2DSceneTexture = Texture(m_renderWidth, m_renderHeight, m_renderScale <= 0.5f ? Texture::TextureType::RetroColor : Texture::TextureType::Data);
			m_2DPostProcessRender = RenderTarget(false);
			m_2DPostProcessRender.bindColorTextureToFrameBuffer(m_lit2DSceneTexture);

			m_2DBackgroundTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_2DBackgroundRender = RenderTarget(false);
			m_2DBackgroundRender.bindColorTextureToFrameBuffer(m_2DBackgroundTexture);

			m_combineResultTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_combinationRender = RenderTarget(false);
			m_combinationRender.bindColorTextureToFrameBuffer(m_combineResultTexture);

			m_outputResolutionRender = RenderTarget(false);

			m_renderPlane = RenderPlane();

			GL_CHECK_ERROR();

			// Enable culling
			glCullFace(GL_FRONT);
			glFrontFace(GL_CCW);
		}

		Renderer::~Renderer()
		{
			// Delete all the objects we've created
			m_geometryShaderProgram.free();
			m_instancedGeometryShaderProgram.free();
			m_2DDistanceShader.free();
			m_2DLightingShader.free();
			m_3DsdfShaderProgram.free();
			m_combineScenesShaderProgram.free();
			m_outputShaderProgram.free();

			m_geometryRender.free();
			m_3DSceneRender.free();
			m_2DSceneRender.free();
			m_2DPostProcessRender.free();
			m_combinationRender.free();
			m_outputResolutionRender.free();

			m_geometryTexture.dispose();
			m_geometryDepthTexture.dispose();
			m_3DSceneTexture.dispose();
			m_distanceTexture.dispose();
			m_lit2DSceneTexture.dispose();
			m_combineResultTexture.dispose();

			delete[] m_2DData;
		}

		void Renderer::render(Scene& scene, const double time)
		{
			if (m_vSyncEnabled)
			{
				SDL_GL_SetSwapInterval(1); // Enable VSync
			}
			else
			{
				SDL_GL_SetSwapInterval(0); // Disable VSync
			}

			// Get camera
			auto& sceneCamera = scene.getCamera();
			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.updateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);

			auto& renderQueue = scene.getDrawQueue(); // TODO: sort and then draw it

			//
			auto renderMode = scene.getRenderMode();
			bool enable2D = renderMode == Scene::RenderMode::RayMarching2D || renderMode == Scene::RenderMode::RayMarchingBoth;
			bool enable3D = renderMode != Scene::RenderMode::RayMarching2D;
			bool used2DAsBackground = renderMode == Scene::RenderMode::RayMarchingBoth;
			bool renderMeshesOnly = renderMode == Scene::RenderMode::Simple3D;

			// TODO: abstract this
			glDisable(GL_DEPTH_TEST);

			// 2D Ray marching
			if (enable2D) {
				// 2D ray marching
				// Renders to m_distanceTexture
				// Stores non-RGBA data in each channel
				// R: Distance
				// G: MaterialID
				// B: Mask for color blending
				// A: Empty
				{
					// Bind the framebuffer you want to render to
					m_2DSceneRender.bind();

					// Draw ray marching stuff
					m_2DDistanceShader.use();

					scene.updateRayMarchingShader(m_2DDistanceShader);





					static auto oldCameraMatrix = sceneCamera.view;

					// Set uniforms
					m_2DDistanceShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DDistanceShader.setUniform("u_oldCamMatrix", oldCameraMatrix);
					oldCameraMatrix = sceneCamera.view;

					static glm::vec3 lastCameraPosition = scene.getCamera().position;
					glm::vec3 cameraPositionChange = scene.getCamera().position - lastCameraPosition;
					lastCameraPosition = scene.getCamera().position;
					m_2DDistanceShader.setUniform("u_camPositionChange", cameraPositionChange);


					m_2DDistanceShader.setUniform("u_time", scene.getTime());

					static double lastTime = time;
					double deltaTime = time - lastTime;
					lastTime = time;
					m_2DDistanceShader.setUniform("u_deltaTime", static_cast<float>(deltaTime));
					m_2DDistanceShader.setUniform("u_resolution", glm::vec2( m_distanceSampleWidth, m_distanceSampleHeight));

					m_2DDistanceShader.setUniform("u_blendIterations", 1);

					m_2DDistanceShader.setUniform("t_colorTexture", 0);
					m_distanceTexture.bind(0);

					// Shape data
					scene.get2DShapesData(m_2DData, m_2DDataSize);
					m_2DDistanceShader.setUniform("u_loadedObjects", (int)m_2DDataSize);

					m_2DDistanceShader.setUniform("t_shapeBuffer", 1);
					m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
					m_shapes2D.bind(1);

					m_renderPlane.draw(m_2DDistanceShader);

					// m_distanceTexture.unbind();
					m_shapes2D.unbind();
				}

				if ( USE_CORRECTED_DISTANCE_TEXTURE) {
					float maxDim = std::max<float>(m_distanceSampleWidth, m_distanceSampleHeight);
					uint16_t m_jumpFloodIterations = largestPowerOfTwoBelow(maxDim);
					bool pingpong = true;

					{
						// Initialize
						m_jumpFloodInitRender.bind();
						m_JumpFloodInitShader.use();

						m_2DDistanceCorrectionShader.setUniform("t_distanceTexture", 0);
						m_distanceTexture.bind(0);

						m_renderPlane.draw(m_JumpFloodInitShader);

						// Jumps
						m_JumpFloodStepShader.use();

						m_JumpFloodStepShader.setUniform("t_prevSeeds", 0);
						m_JumpFloodStepShader.setUniform("u_texelSize", glm::vec2(1.0f / m_distanceSampleWidth, 1.0 / m_distanceSampleHeight));

						float jump = m_jumpFloodIterations;
						bool first = true;

						while (jump >= 1)
						{
							m_JumpFloodDoubleBuffer[pingpong]->bind();

							// Update uniforms
							vec2 uJumpSize;
							uJumpSize.x = float(jump) / float(m_distanceSampleWidth);
							uJumpSize.y = float(jump) / float(m_distanceSampleHeight);
							m_JumpFloodStepShader.setUniform("u_jumpSize", uJumpSize);

							if (first)
							{
								m_jumpFloodInitTexture.bind(0);
								first = false;
							}
							else
							{
								m_JumpFloodDoubleBuffer[!pingpong]->getColorAttachment()->bind(0);
							}

							m_renderPlane.draw(m_JumpFloodStepShader);

							pingpong = !pingpong;

							jump /= 2;
						}
					}

					{
						m_2DDistanceCorrectionRender.bind();
						m_2DDistanceCorrectionShader.use();
						m_2DDistanceCorrectionShader.setUniform("u_resolution", glm::vec2(m_distanceSampleWidth, m_distanceSampleWidth));
						m_2DDistanceCorrectionShader.setUniform("u_time", scene.getTime());

						// Distance
						m_2DDistanceCorrectionShader.setUniform("t_originalDistanceTexture", 0);
						m_distanceTexture.bind(0);

						// Distance
						m_2DDistanceCorrectionShader.setUniform("t_distanceTexture", 1);

						int lastIndex = pingpong ? 0 : 1; // TODO: check if its the other way
						m_JumpFloodDoubleBuffer[lastIndex]->getColorAttachment()->bind(1);

						m_renderPlane.draw(m_2DDistanceCorrectionShader);
					}
				}

				{
					// Bind the framebuffer you want to render to
					m_2DDistanceUpscaler.bind();

					// Calculate pixel color
					m_2DDistanceUpscalerShader.use();

					// Set uniforms
					m_2DDistanceUpscalerShader.setUniform("u_originalResolution", vec2(m_distanceSampleWidth, m_distanceSampleHeight));
					m_2DDistanceUpscalerShader.setUniform("u_targetResolution", vec2(m_renderWidth, m_renderHeight));
					m_2DDistanceUpscalerShader.setUniform("t_data", 0);

					m_distanceTexture.bind(0);

					m_renderPlane.draw(m_2DDistanceUpscalerShader);
				}

				// Renders color
				// RGB: color
				// A: mask used for next step
				{
					// Bind the framebuffer you want to render to
					m_2DColorRender.bind();

					// Calculate pixel color
					m_2DMaterialColorShader.use();

					// Get materials ?
					// scene.updateRayMarchingShader(m_2DcolorShaderProgram);

					// Set uniforms
					m_2DMaterialColorShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DMaterialColorShader.setUniform("u_time", scene.getTime());
					m_2DMaterialColorShader.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));
					m_2DMaterialColorShader.setUniform("u_staticColors", m_colorPalette, 16);

					m_2DMaterialColorShader.setUniform("t_materialDataTexture", 0);
					m_2DDistanceUpscaled.bind(0);

					m_2DMaterialColorShader.setUniform("t_currentColorTexture", 1);
					m_postProcessDoubleBuffer[0]->getColorAttachment()->bind(1);


					m_renderPlane.draw(m_2DMaterialColorShader);

					// TODO: add custom materials, they will be rendered in this same render target with a mask

					m_distanceTexture.unbind();
					m_shapes2D.unbind();
				}

				// Apply gaussian blur to color texture to blend materials
				static bool horizontal = true;
				{
					m_2DMaterialBlendShader.use();
					m_2DMaterialBlendShader.setUniform("t_colorTexture", 0);
					m_2DMaterialBlendShader.setUniform("u_time", scene.getTime());

					for (unsigned int i = 0; i < m_materialBlendIterations * 2; i++)
					{
						m_postProcessDoubleBuffer[horizontal]->bind();

						m_2DMaterialBlendShader.setUniform("u_horizontal", horizontal);
						if (i == 0)
						{
							m_2dColorTexture.bind(0);
						}
						else
						{
							m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
						}

						m_renderPlane.draw(m_2DMaterialBlendShader);

						horizontal = !horizontal;
					}
				}

				// Render background
				{
					m_2DBackgroundRender.bind();

					m_2DGridShader.use();
					m_2DGridShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DGridShader.setUniform("u_time", scene.getTime());
					m_2DGridShader.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					m_renderPlane.draw(m_2DLightingShader);
				}

				// 2D Lighting
				{
					m_2DPostProcessRender.bind();

					m_2DLightingShader.use();
					m_2DLightingShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DLightingShader.setUniform("u_time", scene.getTime());
					m_2DLightingShader.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					// Color texture
					m_2DLightingShader.setUniform("t_colorTexture", 0);
					if (m_materialBlendIterations > 0) {
						m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
					}else {
						m_2dColorTexture.bind(0);
					}


					// Distance
					m_2DLightingShader.setUniform("t_distanceTexture", 1);
					m_2DDistanceUpscaled.bind(1);

					// Bg
					m_2DLightingShader.setUniform("t_backgroundTexture", 2);
					m_2DBackgroundTexture.bind(2);

					// Corrected distance for shadows
					m_2DLightingShader.setUniform("t_shadowDistanceTexture", 3);
					// TODO: dont pass the texture twice if corrected distance is disabled...
					if(USE_CORRECTED_DISTANCE_TEXTURE)
						m_distanceTextureCorrected.bind(3);
					else
						m_distanceTexture.bind(3);


					m_renderPlane.draw(m_2DLightingShader);
					m_postProcessTextureFront.unbind();
				}
			}

			if (!enable3D)
			{
				output(scene, m_lit2DSceneTexture);
				return;
			}

			// Render geometry
			{
				// Set up framebuffer for 3D scene rendering
				glBindFramebuffer(GL_FRAMEBUFFER, m_3DSceneRender.getFrameBuffer());
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear color and depth buffer
				glClearColor(0, 0, 0, 1);

				// Enable culling and depth testing for 3D meshes
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS); // Ensure depth comparison is 'less than'
				glDepthMask(GL_TRUE); // Write to depth buffer

				// Render ray marching only for background (do not affect depth buffer)
				if (!renderMeshesOnly)
				{
					// glDepthMask(GL_FALSE); // Disable depth writing during ray marching
					glEnable(GL_BLEND);
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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
					scene.get2DShapesData(m_2DData, m_2DDataSize);
					m_3DsdfShaderProgram.setUniform("t_shapeBuffer", 2);
					m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
					m_shapes2D.bind(2);

					m_3DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

					GL_CHECK_ERROR();

					// Draw the render plane with ray marching shader
					m_renderPlane.draw(m_3DsdfShaderProgram);

					// Unbind textures and buffers
					m_geometryTexture.unbind();
					m_geometryDepthTexture.unbind();
					m_shapes2D.unbind();

					// glDepthMask(GL_TRUE); // Re-enable depth writing after ray marching
					glDisable(GL_BLEND);
				}

				// Render 3D geometry objects (with depth writing)
				m_geometryShaderProgram.use();
				m_geometryShaderProgram.setUniform("u_camMatrix", sceneCamera.cameraMatrix);
				m_geometryShaderProgram.setUniform("u_camPos", sceneCamera.position);

				auto& lights = scene.getLigths();
				m_geometryShaderProgram.setUniform("u_lightPos", lights[0].position);
				m_geometryShaderProgram.setUniform("u_lightDirection", lights[0].rotation);
				m_geometryShaderProgram.setUniform("u_lightColor", lights[0].color);

				// Draw objects in the scene (3D models)
				scene.renderModels(m_3DSceneRender, m_geometryShaderProgram, m_instancedGeometryShaderProgram);

				glDisable(GL_DEPTH_TEST); // No depth test for the final pass
				glDepthMask(GL_TRUE); // Still write to depth buffer for the 3D meshes
				glClearDepth(1.0f); // Make sure depth buffer is initialized correctly

				GL_CHECK_ERROR();
			}

			if (!used2DAsBackground)
			{
				output(scene, m_3DSceneTexture);
				return;
			}

			// Combine 2D and 3D
			m_combinationRender.bind();

			m_combineScenesShaderProgram.use();
			m_combineScenesShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

			m_combineScenesShaderProgram.setUniform("t_2DSceneTexture", 0);
			m_lit2DSceneTexture.bind(0);

			m_combineScenesShaderProgram.setUniform("t_3DSceneTexture", 1);
			m_3DSceneTexture.bind(1);

			m_renderPlane.draw(m_2DLightingShader);

			m_lit2DSceneTexture.unbind();
			m_3DSceneTexture.unbind();

			output(scene, m_combineResultTexture);
		}



		void Renderer::setWindowTitle(const char* name)
		{
			if (m_window)
			{
				SDL_SetWindowTitle(m_window, name);
			}
		}

		void Renderer::output(Scene& scene, Texture& texture)
		{
			// TODO: abstract this
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_windowWidth, m_windowHeight);

			m_outputShaderProgram.use();
			m_outputShaderProgram.setUniform("u_time", scene.getTime());
			m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
			m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

			m_outputShaderProgram.setUniform("t_colorTexture", 0);
			texture.bind(0);

			m_renderPlane.draw(m_outputShaderProgram);

			texture.unbind();

			// Screenshot
			if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKeyDown(Input::S))
			{
				texture.saveToDisk("output_texture.png");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::L))
			{
				m_2DLightingShader.toggleDefine("SHADOWS_ENABLED");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::D))
			{
				if (Input::GetKey(Input::LeftShift)) {
					m_2DLightingShader.toggleDefine("DITHERING");
				} else {
					m_2DLightingShader.toggleDefine("DEBUG_SHOW_DISTANCE");
				}
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::C))
			{
				m_2DLightingShader.toggleDefine("DEBUG_SHOW_COLORS");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::A))
			{
				m_2DLightingShader.toggleDefine("ANTIALIASING");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::B))
			{
				m_2DDistanceShader.toggleDefine("BLEND_SHAPES");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::M))
			{
				m_2DDistanceShader.toggleDefine("MOTION_BLUR");
			}

			if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::F))
			{
				USE_CORRECTED_DISTANCE_TEXTURE = !USE_CORRECTED_DISTANCE_TEXTURE;
			}

			if (scene.m_collisionSoundQueued)
			{
				// m_audioEngine.triggerCollision(220.0f, 0.2f, 0.15f);
				scene.m_collisionSoundQueued = false;
			}


			// Update friction sound
			float frictionValue = scene.getFrictionSound();
			m_audioEngine.setFrictionLevel(frictionValue);


			// std::cout << "frictionValue: " << frictionValue << std::endl;

			SDL_GL_SwapWindow(m_window);

			GL_CHECK_ERROR();
		}

		SDL_Window* Renderer::getWindow()
		{
			return m_window;
		}

	}
}