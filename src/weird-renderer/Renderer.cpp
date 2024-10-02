#include "Renderer.h"


namespace WeirdRenderer
{

	GLuint framebuffer1, framebuffer2;
	GLuint texture1, texture2;

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

		m_postProcessShaderProgram = Shader("src/weird-renderer/shaders/raymarching.vert", "src/weird-renderer/shaders/postProcess2d.frag");
		m_postProcessShaderProgram.activate();

		m_outputShaderProgram = Shader("src/weird-renderer/shaders/raymarching.vert", "src/weird-renderer/shaders/output.frag");
		m_outputShaderProgram.activate();

		// A plane that takes 100% of the screen and displays the result of a shader
		m_sdfRenderPlane = RenderPlane(m_renderWidth, m_renderHeight, GL_LINEAR, m_sdfShaderProgram, true);
		m_outputRenderPlane = RenderPlane(m_windowWidth, m_windowWidth, GL_NEAREST, m_outputShaderProgram, false);
		m_postProcessRenderPlane = RenderPlane(m_renderWidth, m_renderHeight, GL_NEAREST, m_postProcessShaderProgram, false);

		texture1 = m_postProcessRenderPlane.m_colorTexture;

		//glGenFramebuffers(1, &framebuffer1);
		//glGenFramebuffers(1, &framebuffer2);

		//// Create textures for each framebuffer
		//glGenTextures(1, &texture1);
		//glGenTextures(1, &texture2);

	

		//// Initialize texture1
		//glBindTexture(GL_TEXTURE_2D, texture1);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_renderWidth, m_renderHeight, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture1, 0);

		//// Initialize texture2
		//glBindTexture(GL_TEXTURE_2D, texture2);
		//glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, m_renderWidth, m_renderHeight, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		//glBindFramebuffer(GL_FRAMEBUFFER, framebuffer2);
		//glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture2, 0);

		//// Ensure framebuffers are complete
		//if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		//	std::cout << "Framebuffer not complete!" << std::endl;

		unsigned char textureData[2][2][3] = 
		{
			{{255, 255, 255}, {0, 0, 0}},
			{{0, 0, 0}, {255, 255, 255}}
		};

		// Generate and bind texture
		glGenTextures(1, &texture2);
		glBindTexture(GL_TEXTURE_2D, texture2);

		// Upload texture data
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

		// Set texture parameters (wrap and filtering)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

		// Unbind texture
		glBindTexture(GL_TEXTURE_2D, 0);

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

		glDeleteTextures(1, &texture1);
		glDeleteTextures(1, &texture2);


		//delete m_sdfRenderPlane;

		// Delete m_window before ending the program
		glfwDestroyWindow(m_window);
		// Terminate GLFW before ending the program
		glfwTerminate();
	}

	bool useFirstFramebuffer = false;



#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb/stb_image_write.h>

	int g_renderedFramesCounter = 0;


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
		glEnable(GL_DEPTH_TEST);

		glViewport(0, 0, m_renderWidth, m_renderHeight);

		auto& sceneCamera = scene.getCamera();
		// Updates and exports the camera matrix to the Vertex Shader
		sceneCamera.UpdateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);

		// Draw objects in scene
		//scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);



		if (!m_renderMeshesOnly) 
		{
			useFirstFramebuffer = !useFirstFramebuffer;

			// Bind to default frame buffer
			//GLuint currentFramebuffer = useFirstFramebuffer ? framebuffer1 : framebuffer2;
			//GLuint previousTexture = useFirstFramebuffer ? texture2 : texture1;

			// Bind the framebuffer you want to render to
			glBindFramebuffer(GL_FRAMEBUFFER, m_sdfRenderPlane.GetFrameBuffer());
			//glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			//glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			//glViewport(0, 0, m_renderWidth, m_renderHeight);


			glDisable(GL_DEPTH_TEST);

			// Draw ray marching stuff
			m_sdfShaderProgram.activate();

			// Magical math to calculate ray marching shader FOV
			float shaderFov = 1.0 / tan(sceneCamera.fov * 0.01745 * 0.5);
			// Set uniforms
			m_sdfShaderProgram.setUniform("u_cameraMatrix", sceneCamera.view);
			m_sdfShaderProgram.setUniform("u_fov", shaderFov);
			m_sdfShaderProgram.setUniform("u_time", time);
			m_sdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

			m_sdfShaderProgram.setUniform("u_blendIterations", g_renderedFramesCounter % 3 == 0 ? 1 : 0);
			g_renderedFramesCounter++;




			// Draw render plane with sdf shader
			scene.renderShapes(m_sdfShaderProgram, m_sdfRenderPlane);


			//texture1 = m_sdfRenderPlane.m_colorTexture;
			//// Post process
			glBindFramebuffer(GL_FRAMEBUFFER, m_postProcessRenderPlane.GetFrameBuffer());

			m_postProcessShaderProgram.activate();
			m_postProcessShaderProgram.setUniform("u_time", time);
			m_postProcessShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));
			
			m_postProcessRenderPlane.m_colorTexture = m_sdfRenderPlane.m_colorTexture;

			m_postProcessRenderPlane.Draw(m_postProcessShaderProgram);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, m_windowWidth, m_windowHeight);

		m_outputShaderProgram.activate();
		m_outputShaderProgram.setUniform("u_time", time);
		m_outputShaderProgram.setUniform("u_resolution", glm::vec2(m_windowWidth, m_windowHeight));
		m_outputShaderProgram.setUniform("u_renderScale", m_renderScale);

		//m_outputRenderPlane.m_colorTexture = useFirstFramebuffer ? texture2 : texture1;
		m_outputRenderPlane.m_colorTexture = texture1;




		glDisable(GL_DEPTH_TEST);
		// Tell OpenGL which Shader Program we want to use
		glUseProgram(m_outputShaderProgram.ID);


		m_outputRenderPlane.Draw(m_outputShaderProgram);

		// Screenshot
		if (Input::GetKeyDown(Input::P)) {
			glBindTexture(GL_TEXTURE_2D, m_postProcessRenderPlane.m_colorTexture);
			int width, height;
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
			glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

			unsigned char* data = new unsigned char[width * height * 4];  // Assuming 4 channels (RGBA)
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			stbi_write_png("output_texture.png", width, height, 4, data, width * 4);

			delete[] data;

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