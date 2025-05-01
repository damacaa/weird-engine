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
			//glEnable(GL_DEBUG_OUTPUT);              // Enable OpenGL debug output
			//glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);  // Ensure synchronous reporting (optional but useful)

			// Set the debug callback function
			//glDebugMessageCallback(OpenGLDebugCallback, nullptr);

		}

		Renderer::Renderer(const unsigned int width, const unsigned int height)
			: m_initializer(width, height, m_window)
			, m_windowWidth(width)
			, m_windowHeight(height)
			, m_renderScale(0.5f)
			, m_renderWidth(width* m_renderScale)
			, m_renderHeight(height* m_renderScale)
			, m_renderMeshesOnly(false)
			, m_vSyncEnabled(true)
		{
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
			m_geometryTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_geometryDepthTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST, true);
			m_geometryRenderPlane = RenderPlane(false);
			m_geometryRenderPlane.BindColorTextureToFrameBuffer(m_geometryTexture);
			m_geometryRenderPlane.BindDepthTextureToFrameBuffer(m_geometryDepthTexture);

			m_3DSceneTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_3DRenderPlane = RenderPlane(false);
			m_3DRenderPlane.BindColorTextureToFrameBuffer(m_3DSceneTexture);

			m_distanceTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_sdfRenderPlane = RenderPlane(false);
			m_sdfRenderPlane.BindColorTextureToFrameBuffer(m_distanceTexture);

			m_lit2DSceneTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_postProcessRenderPlane = RenderPlane(false);
			m_postProcessRenderPlane.BindColorTextureToFrameBuffer(m_lit2DSceneTexture);

			m_combineResultTexture = Texture(m_renderWidth, m_renderHeight, GL_NEAREST);
			m_combinationRenderPlane = RenderPlane(false);
			m_combinationRenderPlane.BindColorTextureToFrameBuffer(m_combineResultTexture);

			m_outputRenderPlane = RenderPlane(false);

			GL_CHECK_ERROR();

			// Enables the Depth Buffer and chooses which depth function to use
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);

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

			m_geometryRenderPlane.Delete();
			m_3DRenderPlane.Delete();
			m_sdfRenderPlane.Delete();
			m_postProcessRenderPlane.Delete();
			m_combinationRenderPlane.Delete();
			m_outputRenderPlane.Delete();

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

			bool enable3D = scene.requires3DRendering();
			bool used2DAsBackground = true;

			//
			glDisable(GL_DEPTH_TEST);

			// Render viewport
			glViewport(0, 0, m_renderWidth, m_renderHeight);


			// 2D Ray marching
			{
				// Bind the framebuffer you want to render to
				glBindFramebuffer(GL_FRAMEBUFFER, m_sdfRenderPlane.GetFrameBuffer()); // m_sdfRenderPlane.GetFrameBuffer()

				// Draw ray marching stuff
				m_2DsdfShaderProgram.activate();

				scene.updateRayMarchingShader(m_2DsdfShaderProgram);

				// Set uniforms
				m_2DsdfShaderProgram.setUniform("u_cameraMatrix", sceneCamera.view);
				m_2DsdfShaderProgram.setUniform("u_time", scene.getTime());
				m_2DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				m_2DsdfShaderProgram.setUniform("u_blendIterations", 1);

				GLuint u_colorTextureLocation = glGetUniformLocation(m_2DsdfShaderProgram.ID, "u_colorTexture");
				glUniform1i(u_colorTextureLocation, 0);

				m_distanceTexture.bind(0);

				scene.get2DShapesData(m_2DData, m_2DDataSize);
				m_2DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

				m_sdfRenderPlane.Bind();

				m_2DsdfShaderProgram.setUniform("u_shapeBuffer", 1);
				m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
				m_shapes2D.bind(1);

				m_sdfRenderPlane.Draw(m_2DsdfShaderProgram);

				m_distanceTexture.unbind();
				m_shapes2D.unbind();
			}

			// 2D Lighting
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_postProcessRenderPlane.GetFrameBuffer());

				m_postProcessShaderProgram.activate();
				m_postProcessShaderProgram.setUniform("u_time", scene.getTime());
				m_postProcessShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				GLuint u_colorTextureLocation = glGetUniformLocation(m_postProcessShaderProgram.ID, "u_colorTexture");
				glUniform1i(u_colorTextureLocation, 0);

				m_distanceTexture.bind(0);
				m_postProcessRenderPlane.Bind();
				m_postProcessRenderPlane.Draw(m_postProcessShaderProgram);
				m_distanceTexture.unbind();
			}

			if (!enable3D)
			{
				output(scene, m_lit2DSceneTexture);
				return;
			}


			// Render geometry
			{
				glBindFramebuffer(GL_FRAMEBUFFER, m_geometryRenderPlane.GetFrameBuffer());

				// Enable depth test
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS);
				glDepthMask(GL_TRUE);

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear everything first

				// Draw objects in scene
				scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);

				glDisable(GL_DEPTH_TEST); // No depth test
				glDepthMask(GL_TRUE); // Still write to depth buffer
				glClearDepth(1.0f); // Make sure depth buffer is initialized
			}

			if (m_renderMeshesOnly)
			{
				output(scene, m_geometryTexture);
				return;
			}

			// 3D Ray marching
			{
				// Bind the framebuffer you want to render to
				glBindFramebuffer(GL_FRAMEBUFFER, m_3DRenderPlane.GetFrameBuffer());

				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

				// Draw ray marching stuff
				m_3DsdfShaderProgram.activate();

				// Magical math to calculate ray marching shader FOV
				float shaderFov = 1.0f / tan(sceneCamera.fov * 0.01745f * 0.5f);

				// Set uniforms
				m_3DsdfShaderProgram.setUniform("u_cameraMatrix", sceneCamera.view);
				m_3DsdfShaderProgram.setUniform("u_fov", shaderFov);
				m_3DsdfShaderProgram.setUniform("u_time", scene.getTime());
				m_3DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				GLuint u_colorTextureLocation = glGetUniformLocation(m_3DsdfShaderProgram.ID, "u_colorTexture");
				glUniform1i(u_colorTextureLocation, 0);
				m_geometryTexture.bind(0);

				GLuint u_depthTextureLocation = glGetUniformLocation(m_3DsdfShaderProgram.ID, "u_depthTexture");
				glUniform1i(u_depthTextureLocation, 1);
				m_geometryDepthTexture.bind(1);

				// Draw render plane with sdf shader
				//scene.renderShapes(m_3DsdfShaderProgram, m_sdfRenderPlane);

				m_3DRenderPlane.Bind();

				m_3DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

				m_3DsdfShaderProgram.setUniform("u_shapeBuffer", 2);
				//m_shapes2D.uploadData<Dot2D>(m_2DData, m_2DDataSize);
				m_shapes2D.bind(2);

				m_3DRenderPlane.Draw(m_3DsdfShaderProgram);

				m_geometryTexture.unbind();
				m_geometryDepthTexture.unbind();
				m_shapes2D.unbind();
			}



			glBindFramebuffer(GL_FRAMEBUFFER, m_combinationRenderPlane.GetFrameBuffer());

			m_combineScenesShaderProgram.activate();
			m_combineScenesShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

			GLuint u_colorTextureLocation2d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "u_2DSceneTexture");
			glUniform1i(u_colorTextureLocation2d, 0);
			if (used2DAsBackground)
			{
				m_lit2DSceneTexture.bind(0);
			}
			else
			{
				m_3DSceneTexture.bind(0);
			}

			GLuint u_colorTextureLocation3d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "u_3DSceneTexture");
			glUniform1i(u_colorTextureLocation3d, 1);
			m_3DSceneTexture.bind(1);

			m_combinationRenderPlane.Bind();
			m_combinationRenderPlane.Draw(m_postProcessShaderProgram);


			m_lit2DSceneTexture.unbind();
			m_3DSceneTexture.unbind();

			output(scene, m_combineResultTexture);
		}

		void Renderer::renderGeometry(Scene& scene, Camera& camera)
		{
			glEnable(GL_CULL_FACE);
			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);

			// Updates and exports the camera matrix to the Vertex Shader
			camera.UpdateMatrix(0.1f, 100.0f, m_windowWidth, m_windowHeight);


			// Draw objects in scene
			scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);

			glDisable(GL_DEPTH_TEST); // No depth test
			glDepthMask(GL_TRUE); // Still write to depth buffer
			glClearDepth(1.0f); // Make sure depth buffer is initialized
			glClear(GL_DEPTH_BUFFER_BIT); // Clear depth
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

			GLuint u_colorTextureLocation = glGetUniformLocation(m_outputShaderProgram.ID, "u_colorTexture");
			glUniform1i(u_colorTextureLocation, 0);

			texture.bind(0);

			m_outputRenderPlane.Bind();
			m_outputRenderPlane.Draw(m_outputShaderProgram);

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