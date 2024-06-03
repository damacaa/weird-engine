#include "Renderer.h"




Renderer::Renderer(const unsigned int width, const unsigned int height):
	m_width(width),
	m_height(height)
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
	m_defaultShaderProgram = Shader("src/weird-renderer/shaders/default.vert", "src/weird-renderer/shaders/default.frag");
	m_defaultShaderProgram.activate();

	// Generates Shader objects
	m_defaultInstancedShaderProgram = Shader("src/weird-renderer/shaders/default_instancing.vert", "src/weird-renderer/shaders/default.frag");
	m_defaultInstancedShaderProgram.activate();

	m_sdfShaderProgram = Shader("src/weird-renderer/shaders/raymarching.vert", "src/weird-renderer/shaders/raymarching.frag");
	m_sdfShaderProgram.activate();

	// A plane that takes 100% of the screen and displays the result of a shader
	m_sdfRenderPlane = RenderPlane(width, height, m_sdfShaderProgram);
}

Renderer::~Renderer()
{
	// Delete all the objects we've created
	m_defaultShaderProgram.Delete();
	m_defaultInstancedShaderProgram.Delete();
	m_sdfShaderProgram.Delete();

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


	if (m_renderMeshesOnly) {
		// Bind to default frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
	else {
		// Bind sdf renderer frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, m_sdfRenderPlane.GetFrameBuffer());
	}

	// Specify the color of the background
	glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
	// Clean the back buffer and depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// Enable depth test
	glEnable(GL_DEPTH_TEST);

	// Updates and exports the camera matrix to the Vertex Shader
	scene.camera->UpdateMatrix(0.1f, 100.0f, m_width, m_height);

	// Draw objects in scene
	scene.renderModels(m_defaultShaderProgram, m_defaultInstancedShaderProgram);

	if (!m_renderMeshesOnly) {
		// Bind to default frame buffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		// Draw ray marching stuff
		m_sdfShaderProgram.activate();

		// Magical math to calculate ray marching shader FOV
		float shaderFov = 1.0 / tan(scene.camera->fov * 0.01745 * 0.5);
		// Set uniforms
		m_sdfShaderProgram.setUniform("u_cameraMatrix", scene.camera->view);
		m_sdfShaderProgram.setUniform("u_fov", shaderFov);
		m_sdfShaderProgram.setUniform("u_time", time);
		m_sdfShaderProgram.setUniform("u_resolution", glm::vec2(m_width, m_height));

		// Draw render plane with sdf shader
		scene.renderShapes(m_sdfShaderProgram, m_sdfRenderPlane);
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

