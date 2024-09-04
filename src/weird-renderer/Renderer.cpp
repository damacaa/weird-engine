#include "Renderer.h"


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

		// Generates Shader objects
		m_geometryShaderProgram = Shader("src/weird-renderer/shaders/default.vert", "src/weird-renderer/shaders/default.frag");
		m_geometryShaderProgram.activate();


		// Generates Shader objects
		m_instancedGeometryShaderProgram = Shader("src/weird-renderer/shaders/default_instancing.vert", "src/weird-renderer/shaders/default.frag");
		m_instancedGeometryShaderProgram.activate();


		m_sdfShaderProgram = Shader("src/weird-renderer/shaders/raymarching.vert", "src/weird-renderer/shaders/raymarching2d.frag");
		m_sdfShaderProgram.activate();

		m_outputShaderProgram = Shader("src/weird-renderer/shaders/raymarching.vert", "src/weird-renderer/shaders/output.frag");
		m_outputShaderProgram.activate();

		// A plane that takes 100% of the screen and displays the result of a shader
		m_sdfRenderPlane = RenderPlane(m_renderWidth, m_renderHeight, m_sdfShaderProgram, true);
		m_outputRenderPlane = RenderPlane(m_renderWidth, m_renderHeight, m_outputShaderProgram, false);
	}

	Renderer::~Renderer()
	{
		// Delete all the objects we've created
		m_geometryShaderProgram.Delete();
		m_instancedGeometryShaderProgram.Delete();
		m_sdfShaderProgram.Delete();
		m_outputShaderProgram.Delete();

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
		glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
		// Clean the back buffer and depth buffer
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Enable depth test
		glEnable(GL_DEPTH_TEST);

		glViewport(0, 0, m_renderWidth, m_renderHeight);

		auto& sceneCamera = scene.getCamera();
		// Updates and exports the m_camera matrix to the Vertex Shader
		sceneCamera.UpdateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);

		// Draw objects in scene
		scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);

		if (!m_renderMeshesOnly) {


			// Bind to default frame buffer
			glBindFramebuffer(GL_FRAMEBUFFER, m_outputRenderPlane.GetFrameBuffer());
			glViewport(0, 0, m_renderWidth, m_renderHeight);

			// Draw ray marching stuff
			m_sdfShaderProgram.activate();

			// Magical math to calculate ray marching shader FOV
			float shaderFov = 1.0 / tan(sceneCamera.fov * 0.01745 * 0.5);
			// Set uniforms
			m_sdfShaderProgram.setUniform("u_cameraMatrix", sceneCamera.view);
			m_sdfShaderProgram.setUniform("u_fov", shaderFov);
			m_sdfShaderProgram.setUniform("u_time", time);
			m_sdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

			// Draw render plane with sdf shader
			scene.renderShapes(m_sdfShaderProgram, m_sdfRenderPlane);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, m_windowWidth, m_windowHeight);

		m_outputShaderProgram.activate();
		m_outputShaderProgram.setUniform("u_time", time);
		m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
		m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

		m_outputRenderPlane.Draw(m_outputShaderProgram);



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