#include "weird-renderer/Renderer.h"

#include <SDL3/SDL.h>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		void GLAPIENTRY OpenGLDebugCallback(
			GLenum source,
			GLenum type,
			GLuint id,
			GLenum severity,
			GLsizei length,
			const GLchar* message,
			const void* userParam)
		{
			// You can filter specific messages by severity or source if needed
			std::cerr << "OpenGL Debug: " << message << std::endl;
		}

		GLInitializer::GLInitializer(const unsigned int width, const unsigned int height, SDL_Window*& m_window)
		{
			// Initialize SDL
			if (!SDL_Init(SDL_INIT_VIDEO))
			{
				std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
				throw;
			}

			// Set OpenGL attributes for a core profile
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
			SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
			SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

			// Create an SDL window that supports OpenGL
			m_window = SDL_CreateWindow("SDL3 OpenGL Window", width, height, SDL_WINDOW_OPENGL);

			if (m_window == NULL)
			{
				std::cerr << "Failed to create SDL window: " << SDL_GetError() << std::endl;
				SDL_Quit();
				throw;
			}

			// Create the OpenGL context
			SDL_GLContext m_glContext = SDL_GL_CreateContext(m_window);
			if (m_glContext == NULL)
			{
				std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
				SDL_DestroyWindow(m_window);
				SDL_Quit();
				throw;
			}

			// Make the OpenGL context current
			SDL_GL_MakeCurrent(m_window, m_glContext);

			// Load GLAD so it configures OpenGL
			if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
			{
				std::cerr << "Failed to initialize GLAD" << std::endl;
				SDL_GL_DestroyContext(m_glContext);
				SDL_DestroyWindow(m_window);
				SDL_Quit();
				throw;
			}

			// Clear any GL errors that may have occurred during initialization
			while (glGetError() != GL_NO_ERROR);

			// Enable OpenGL debug output
			glEnable(GL_DEBUG_OUTPUT);
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallback(OpenGLDebugCallback, nullptr);
		}

		Renderer::Renderer(const unsigned int width, const unsigned int height)
			: m_initializer(width, height, m_window)
			, m_windowWidth(width)
			, m_windowHeight(height)
			, m_renderScale(1.0f)
			, m_renderWidth(width * m_renderScale)
			, m_renderHeight(height * m_renderScale)
			, m_vSyncEnabled(true)
			, m_materialBlendIterations(10)
		{
			Screen::width = m_windowWidth;
			Screen::height = m_windowHeight;
			Screen::rWidth = m_renderWidth;
			Screen::rHeight = m_renderHeight;

			// Load shaders
			m_geometryShaderProgram = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "default.frag");

			m_instancedGeometryShaderProgram = Shader(SHADERS_PATH "default_instancing.vert", SHADERS_PATH "default.frag");

			m_3DsdfShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "raymarching.frag");

			m_2DDistanceShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DSDFDistanceShader.frag");

			m_2DMaterialColorShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialColorShader.frag");

			m_2DMaterialBlendShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DMaterialBlendShader.frag");

			m_2DLightingShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "2DLightingShader.frag");

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


			m_distanceTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
			m_2DSceneRender = RenderTarget(false);
			m_2DSceneRender.bindColorTextureToFrameBuffer(m_distanceTexture);

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

			m_lit2DSceneTexture = Texture(m_renderWidth, m_renderHeight, Texture::TextureType::Data);
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

			// SDL_DestroyRenderer(renderer); // TODO
			SDL_DestroyWindow(m_window);
			SDL_Quit();
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

			//
			auto renderMode = scene.getRenderMode();
			bool enable2D = renderMode == Scene::RenderMode::RayMarching2D || renderMode == Scene::RenderMode::RayMarchingBoth;
			bool enable3D = renderMode != Scene::RenderMode::RayMarching2D;
			bool used2DAsBackground = renderMode == Scene::RenderMode::RayMarchingBoth;
			bool renderMeshesOnly = renderMode == Scene::RenderMode::Simple3D;

			//
			glDisable(GL_DEPTH_TEST);

			// Render viewport
			glViewport(0, 0, m_renderWidth, m_renderHeight);

			// 2D Ray marching
			if (enable2D)
			{
				{
					// Bind the framebuffer you want to render to
					m_2DSceneRender.bind();

					// Draw ray marching stuff
					m_2DDistanceShader.use();

					scene.updateRayMarchingShader(m_2DDistanceShader);

					// Set uniforms
					m_2DDistanceShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DDistanceShader.setUniform("u_time", scene.getTime());
					m_2DDistanceShader.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

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

					m_distanceTexture.unbind();
					m_shapes2D.unbind();
				}

				{
					// Bind the framebuffer you want to render to
					m_2DColorRender.bind();

					// Draw ray marching stuff
					m_2DMaterialColorShader.use();

					// Get materials ?
					// scene.updateRayMarchingShader(m_2DcolorShaderProgram);

					// Set uniforms
					m_2DMaterialColorShader.setUniform("u_camMatrix", sceneCamera.view);
					m_2DMaterialColorShader.setUniform("u_time", scene.getTime());
					m_2DMaterialColorShader.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));
					m_2DMaterialColorShader.setUniform("u_staticColors", m_colorPalette, 16);

					m_2DMaterialColorShader.setUniform("t_colorTexture", 0);
					m_distanceTexture.bind(0);


					m_renderPlane.draw(m_2DMaterialColorShader);

					m_distanceTexture.unbind();
					m_shapes2D.unbind();
				}

				bool horizontal = true;
				{
					m_2DMaterialBlendShader.use();
					m_2DMaterialBlendShader.setUniform("t_colorTexture", 0);

					for (unsigned int i = 0; i < m_materialBlendIterations; i++)
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

					GLuint colorTextureLocation = glGetUniformLocation(m_2DLightingShader.ID, "t_colorTexture");
					glUniform1i(colorTextureLocation, 0);

					m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);

					GLuint distanceTextureLocation = glGetUniformLocation(m_2DLightingShader.ID, "t_distanceTexture");
					glUniform1i(distanceTextureLocation, 1);
					m_distanceTexture.bind(1);

					GLuint backgroundTextureLocation = glGetUniformLocation(m_2DLightingShader.ID, "t_backgroundTexture");
					glUniform1i(backgroundTextureLocation, 2);
					m_2DBackgroundTexture.bind(2);

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

			GLuint u_colorTextureLocation2d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_2DSceneTexture");
			glUniform1i(u_colorTextureLocation2d, 0);
			m_lit2DSceneTexture.bind(0);

			GLuint u_colorTextureLocation3d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_3DSceneTexture");
			glUniform1i(u_colorTextureLocation3d, 1);
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
			if (Input::GetKeyDown(Input::O))
			{
				texture.saveToDisk("output_texture.png");
			}


			SDL_GL_SwapWindow(m_window);

			GL_CHECK_ERROR();
		}

		SDL_Window* Renderer::getWindow()
		{
			return m_window;
		}

	}
}