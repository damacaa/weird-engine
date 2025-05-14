#include "weird-renderer/Renderer.h"




namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		inline void CheckOpenGLError(const char* file, int line)
		{
			GLenum err;
			while ((err = glGetError()) != GL_NO_ERROR)
			{
				//int e = err;
				std::cerr << "OpenGL Error (" << err << ") at " << file << ":" << line << std::endl;
				// Optionally, map err to a string representation
			}
		}

#define GL_CHECK_ERROR() CheckOpenGLError(__FILE__, __LINE__)

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

		GLInitializer::GLInitializer(const unsigned int width, const unsigned int height, GLFWwindow*& m_window)
		{
			// Initialize GLFW
			glfwInit();

			// Tell GLFW what version of OpenGL we are using
			// In this case we are using OpenGL 3.3
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
			// Tell GLFW we are using the CORE profile
			// So that means we only have the modern functions
			glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

			glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

			// Create a GLFWwindow object
			m_window = glfwCreateWindow(width, height, "Weird", NULL, NULL);
			// Error check if the m_window fails to create
			if (m_window == NULL)
			{
				std::cout << "Failed to create GLFW window" << std::endl;
				glfwTerminate();
				throw;
			}

			// Introduce the m_window into the current context
			glfwMakeContextCurrent(m_window);

			// Load GLAD so it configures OpenGL
			gladLoadGL();

			// Clear any GL errors caused during init
			while (glGetError() != GL_NO_ERROR) {}

			// Debug-specific code
			glEnable(GL_DEBUG_OUTPUT);              // Enable OpenGL debug output
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);  // Ensure synchronous reporting (optional but useful)

			// Set the debug callback function
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
		{
			Screen::width = m_windowWidth;
			Screen::height = m_windowHeight;
			Screen::rWidth = m_renderWidth;
			Screen::rHeight = m_renderHeight;

			// Load shaders
			m_geometryShaderProgram = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "default.frag");

			m_instancedGeometryShaderProgram = Shader(SHADERS_PATH "default_instancing.vert", SHADERS_PATH "default.frag");

			m_3DsdfShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "raymarching.frag");

			m_2DsdfShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "raymarching2d.frag");

			m_postProcessShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "postProcess2d.frag");

			m_combineScenesShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "combineScenes.frag");

			m_outputShaderProgram = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "output.frag");

			GL_CHECK_ERROR();

			// Bind textures to render planes fbo outputs
			m_geometryTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_geometryDepthTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR, true);
			m_geometryRender = RenderTarget(false);
			m_geometryRender.BindColorTextureToFrameBuffer(m_geometryTexture);
			m_geometryRender.BindDepthTextureToFrameBuffer(m_geometryDepthTexture);

			m_3DSceneTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_3DSceneRender = RenderTarget(false);
			m_3DSceneRender.BindColorTextureToFrameBuffer(m_3DSceneTexture);

			m_distanceTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_2DSceneRender = RenderTarget(false);
			m_2DSceneRender.BindColorTextureToFrameBuffer(m_distanceTexture);

			m_lit2DSceneTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_2DPostProcessRender = RenderTarget(false);
			m_2DPostProcessRender.BindColorTextureToFrameBuffer(m_lit2DSceneTexture);

			m_combineResultTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_combinationRender = RenderTarget(false);
			m_combinationRender.BindColorTextureToFrameBuffer(m_combineResultTexture);

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
			m_geometryShaderProgram.Delete();
			m_instancedGeometryShaderProgram.Delete();
			m_2DsdfShaderProgram.Delete();
			m_postProcessShaderProgram.Delete();
			m_3DsdfShaderProgram.Delete();
			m_combineScenesShaderProgram.Delete();
			m_outputShaderProgram.Delete();

			m_geometryRender.Delete();
			m_3DSceneRender.Delete();
			m_2DSceneRender.Delete();
			m_2DPostProcessRender.Delete();
			m_combinationRender.Delete();
			m_outputResolutionRender.Delete();

			m_geometryTexture.dispose();
			m_geometryDepthTexture.dispose();
			m_3DSceneTexture.dispose();
			m_distanceTexture.dispose();
			m_lit2DSceneTexture.dispose();
			m_combineResultTexture.dispose();

			delete[] m_2DData;

			// Delete m_window before ending the program
			glfwDestroyWindow(m_window);
			// Terminate GLFW before ending the program
			glfwTerminate();
		}



		void Renderer::render(Scene& scene, const double time)
		{
			if (m_vSyncEnabled)
			{
				glfwSwapInterval(1);
			}
			else
			{
				glfwSwapInterval(0);
			}

			// Get camera
			auto& sceneCamera = scene.getCamera();
			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.UpdateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);


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
					m_2DSceneRender.Bind();

					// Draw ray marching stuff
					m_2DsdfShaderProgram.activate();

					scene.updateRayMarchingShader(m_2DsdfShaderProgram);

					// Set uniforms
					m_2DsdfShaderProgram.setUniform("u_camMatrix", sceneCamera.view);
					m_2DsdfShaderProgram.setUniform("u_time", scene.getTime());
					m_2DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					m_2DsdfShaderProgram.setUniform("u_blendIterations", 1);

					m_2DsdfShaderProgram.setUniform("t_colorTexture", 0);
					m_distanceTexture.bind(0);

					// Shape data
					scene.get2DShapesData(m_2DData, m_2DDataSize);
					m_2DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

					m_2DsdfShaderProgram.setUniform("t_shapeBuffer", 1);
					m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
					m_shapes2D.bind(1);

					m_renderPlane.Draw(m_2DsdfShaderProgram);

					m_distanceTexture.unbind();
					m_shapes2D.unbind();
				}

				// 2D Lighting
				{
					m_2DPostProcessRender.Bind();

					m_postProcessShaderProgram.activate();
					m_postProcessShaderProgram.setUniform("u_time", scene.getTime());
					m_postProcessShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					GLuint u_colorTextureLocation = glGetUniformLocation(m_postProcessShaderProgram.ID, "t_colorTexture");
					glUniform1i(u_colorTextureLocation, 0);

					m_distanceTexture.bind(0);
					
					m_renderPlane.Draw(m_postProcessShaderProgram);
					m_distanceTexture.unbind();
				}
			}

			if (!enable3D)
			{
				output(scene, m_lit2DSceneTexture);
				return;
			}


			// Render geometry
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_geometryRender.GetFrameBuffer());
				GL_CHECK_ERROR();
				renderGeometry(scene, sceneCamera);
				GL_CHECK_ERROR();
			}

			if (renderMeshesOnly)
			{
				output(scene, m_geometryTexture);
				return;
			}

			// 3D Ray marching
			{
				// Bind the framebuffer you want to render to
				m_3DSceneRender.Bind();

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				// Draw ray marching stuff
				m_3DsdfShaderProgram.activate();

				// Magical math to calculate ray marching shader FOV
				float shaderFov = 1.0f / tan(sceneCamera.fov * 0.01745f * 0.5f);

				// Set uniforms
				m_3DsdfShaderProgram.setUniform("u_camMatrix", sceneCamera.view);
				m_3DsdfShaderProgram.setUniform("u_fov", shaderFov);
				m_3DsdfShaderProgram.setUniform("u_time", scene.getTime());
				m_3DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				auto& lights = scene.getLigths();

				m_3DsdfShaderProgram.setUniform("u_lightPos", lights[0].position);
				m_3DsdfShaderProgram.setUniform("u_lightDirection", lights[0].rotation);
				m_3DsdfShaderProgram.setUniform("u_lightColor", lights[0].color);

				// Geom color
				m_3DsdfShaderProgram.setUniform("t_colorTexture", 0);
				m_geometryTexture.bind(0);

				// Geom depth
				m_3DsdfShaderProgram.setUniform("t_depthTexture", 1);
				m_geometryDepthTexture.bind(1);

				// Shapes

				if (!enable2D) // TODO: Replace this with a 3D data buffer??
				{
					// Shape data
					scene.get2DShapesData(m_2DData, m_2DDataSize);

					m_3DsdfShaderProgram.setUniform("t_shapeBuffer", 2);
					m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
					m_shapes2D.bind(2);

					m_3DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);
				}

				GL_CHECK_ERROR();


				// Draw render plane with sdf shader
				m_renderPlane.Draw(m_3DsdfShaderProgram);

				// Unbind stuff
				m_geometryTexture.unbind();
				m_geometryDepthTexture.unbind();
				m_shapes2D.unbind();
			}

			if (!used2DAsBackground)
			{
				output(scene, m_3DSceneTexture);
				return;
			}
			
			// Combine 2D and 3D
			m_combinationRender.Bind();

			m_combineScenesShaderProgram.activate();
			m_combineScenesShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

			GLuint u_colorTextureLocation2d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_2DSceneTexture");
			glUniform1i(u_colorTextureLocation2d, 0);
			m_3DSceneTexture.bind(0);

			GLuint u_colorTextureLocation3d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_3DSceneTexture");
			glUniform1i(u_colorTextureLocation3d, 1);
			m_3DSceneTexture.bind(1);

			
			m_renderPlane.Draw(m_postProcessShaderProgram);


			m_lit2DSceneTexture.unbind();
			m_3DSceneTexture.unbind();

			output(scene, m_combineResultTexture);
		}



		

		void Renderer::renderFire(Scene& scene, Camera& camera, float time)
		{
			
		}

		void Renderer::renderGeometry(Scene& scene, Camera& camera)
		{
			// Enable depth test
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear everything first
			glClearColor(0, 0, 0, 1);

			m_geometryShaderProgram.activate();
			camera.Matrix(m_geometryShaderProgram, "u_camMatrix");
			m_geometryShaderProgram.setUniform("u_camPos", camera.Position);

			auto& lights = scene.getLigths();

			m_geometryShaderProgram.setUniform("u_lightPos", lights[0].position);
			m_geometryShaderProgram.setUniform("u_lightDirection", lights[0].rotation);
			m_geometryShaderProgram.setUniform("u_lightColor", lights[0].color);

			// Draw objects in scene
			scene.renderModels(m_geometryRender, m_geometryShaderProgram, m_instancedGeometryShaderProgram);

			glDisable(GL_DEPTH_TEST); // No depth test
			glDepthMask(GL_TRUE); // Still write to depth buffer
			glClearDepth(1.0f); // Make sure depth buffer is initialized
		}

		bool Renderer::checkWindowClosed() const
		{
			return glfwWindowShouldClose(m_window);
		}

		void Renderer::setWindowTitle(const char* name)
		{
			glfwSetWindowTitle(m_window, name);
		}

		void Renderer::output(Scene& scene, Texture& texture)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_windowWidth, m_windowHeight);

			m_outputShaderProgram.activate();
			m_outputShaderProgram.setUniform("u_time", scene.getTime());
			m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
			m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

			m_outputShaderProgram.setUniform("t_colorTexture", 0);
			texture.bind(0);

			m_renderPlane.Draw(m_outputShaderProgram);

			texture.unbind();

			// Screenshot
			if (Input::GetKeyDown(Input::O))
			{
				texture.saveToDisk("output_texture.png");
			}

			// Swap the back buffer with the front buffer
			glfwSwapBuffers(m_window);
			// Take care of all GLFW events
			glfwPollEvents();

			GL_CHECK_ERROR();
		}

		GLFWwindow* Renderer::getWindow()
		{
			return m_window;
		}



	}
}