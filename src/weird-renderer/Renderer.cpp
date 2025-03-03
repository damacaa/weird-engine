#include "weird-renderer/Renderer.h"



namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		Renderer::Renderer(const unsigned int width, const unsigned int height) :
			m_windowWidth(width),
			m_windowHeight(height),
			m_renderWidth(width* m_renderScale),
			m_renderHeight(height* m_renderScale),
			m_renderMeshesOnly(false)
		{
			// Initialize GLFW
			glfwInit();

			// Tell GLFW what version of OpenGL we are using 
			// In this case we are using OpenGL 3.3
			glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
			glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
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

			//Load GLAD so it configures OpenGL
			gladLoadGL();
			// Specify the viewport of OpenGL in the Window
			// In this case the viewport goes from x = 0, y = 0, to x = 800, y = 800
			glViewport(0, 0, width, height);

			// Enables the Depth Buffer and choses which depth function to use
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);

			// Enable culling
			//glEnable(GL_CULL_FACE);
			glCullFace(GL_FRONT);
			glFrontFace(GL_CCW);

			glm::vec4 lightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
			glm::vec3 lightPos = glm::vec3(10.5f, 0.5f, 0.5f);

			// Load shaders
			m_geometryShaderProgram = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "default.frag");
			m_geometryShaderProgram.activate();

			m_instancedGeometryShaderProgram = Shader(SHADERS_PATH "default_instancing.vert", SHADERS_PATH "default.frag");
			m_instancedGeometryShaderProgram.activate();


			m_sdfShaderProgram = Shader(SHADERS_PATH "raymarching.vert", SHADERS_PATH "raymarching2d.frag");
			m_sdfShaderProgram.activate();

			m_postProcessShaderProgram = Shader(SHADERS_PATH "raymarching.vert", SHADERS_PATH "postProcess2d.frag");
			m_postProcessShaderProgram.activate();

			m_outputShaderProgram = Shader(SHADERS_PATH "raymarching.vert", SHADERS_PATH "output.frag");
			m_outputShaderProgram.activate();


			// Bind textures to render planes fbo outputs
			m_distanceTexture = Texture(m_renderWidth, m_renderHeight, GL_LINEAR);
			m_sdfRenderPlane = RenderPlane(true);
			m_sdfRenderPlane.BindColorTextureToFrameBuffer(m_distanceTexture);

			m_outputTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_postProcessRenderPlane = RenderPlane(false);
			m_postProcessRenderPlane.BindColorTextureToFrameBuffer(m_outputTexture);

			m_outputRenderPlane = RenderPlane(false);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		Renderer::~Renderer()
		{
			// Delete all the objects we've created
			m_geometryShaderProgram.Delete();
			m_instancedGeometryShaderProgram.Delete();
			m_sdfShaderProgram.Delete();
			m_postProcessShaderProgram.Delete();
			m_outputShaderProgram.Delete();

			m_distanceTexture.dispose();
			m_outputTexture.dispose();

			//delete m_sdfRenderPlane;

			// Delete m_window before ending the program
			glfwDestroyWindow(m_window);
			// Terminate GLFW before ending the program
			glfwTerminate();
		}


		void Renderer::render(Scene& scene, const double time)
		{
			if (m_vSyncEnabled)
				glfwSwapInterval(1);
			else
				glfwSwapInterval(0);

			auto fbo = m_renderMeshesOnly ? m_outputRenderPlane.GetFrameBuffer() : m_sdfRenderPlane.GetFrameBuffer();
			glBindFramebuffer(GL_FRAMEBUFFER, fbo);

			// Specify the color of the background
			//glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
			// Clean the back buffer and depth buffer
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			// Enable depth test

			glViewport(0, 0, m_renderWidth, m_renderHeight);

			auto& sceneCamera = scene.getCamera();
			// Updates and exports the camera matrix to the Vertex Shader
			sceneCamera.UpdateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);


			glEnable(GL_DEPTH_TEST);
			// Draw objects in scene
			//scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);
			glDisable(GL_DEPTH_TEST);


			if (!m_renderMeshesOnly)
			{
				{
					// Bind the framebuffer you want to render to
					glBindFramebuffer(GL_FRAMEBUFFER, m_sdfRenderPlane.GetFrameBuffer()); //m_sdfRenderPlane.GetFrameBuffer()

					// Draw ray marching stuff
					m_sdfShaderProgram.activate();

					// Magical math to calculate ray marching shader FOV
					float shaderFov = 1.0 / tan(sceneCamera.fov * 0.01745 * 0.5);
					// Set uniforms
					m_sdfShaderProgram.setUniform("u_cameraMatrix", sceneCamera.view);
					m_sdfShaderProgram.setUniform("u_fov", shaderFov);
					m_sdfShaderProgram.setUniform("u_time", scene.getTime());
					m_sdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					m_sdfShaderProgram.setUniform("u_blendIterations", 1);

					GLuint u_colorTextureLocation = glGetUniformLocation(m_sdfShaderProgram.ID, "u_colorTexture");
					glUniform1i(u_colorTextureLocation, 0);

					// Draw render plane with sdf shader
					m_distanceTexture.bind(0);
					m_outputTexture.bind(1);

					scene.renderShapes(m_sdfShaderProgram, m_sdfRenderPlane);
				}



				// Post process
				{
					glBindFramebuffer(GL_FRAMEBUFFER, m_postProcessRenderPlane.GetFrameBuffer());

					m_postProcessShaderProgram.activate();
					m_postProcessShaderProgram.setUniform("u_time", scene.getTime());
					m_postProcessShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

					GLuint u_colorTextureLocation = glGetUniformLocation(m_postProcessShaderProgram.ID, "u_colorTexture");
					glUniform1i(u_colorTextureLocation, 0);

					m_distanceTexture.bind(0);
					m_postProcessRenderPlane.Draw(m_postProcessShaderProgram);
				}
			}


			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, m_windowWidth, m_windowHeight);

			m_outputShaderProgram.activate();
			m_outputShaderProgram.setUniform("u_time", scene.getTime());
			m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
			m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

			// Tell OpenGL which Shader Program we want to use
			glUseProgram(m_outputShaderProgram.ID);

			GLuint u_colorTextureLocation = glGetUniformLocation(m_outputShaderProgram.ID, "u_colorTexture");
			glUniform1i(u_colorTextureLocation, 0);

			m_outputTexture.bind(0);

			m_outputRenderPlane.Draw(m_outputShaderProgram);


			// Screenshot
			if (Input::GetKeyDown(Input::O)) {

				m_distanceTexture.saveToDisk("output_texture.png");
			}

			// Swap the back buffer with the front buffer
			glfwSwapBuffers(m_window);
			// Take care of all GLFW events
			glfwPollEvents();
		}

		bool Renderer::checkWindowClosed() const
		{
			return glfwWindowShouldClose(m_window);
		}

		void Renderer::setWindowTitle(const char* name)
		{
			glfwSetWindowTitle(m_window, name);
		}

		GLFWwindow* Renderer::getWindow()
		{
			return m_window;
		}

	}
}