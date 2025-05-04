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

			//// Set the debug callback function
			//glDebugMessageCallback(OpenGLDebugCallback, nullptr);

		}

		bool g_fire = true;
		ResourceManager g_resourceManager;
		std::vector<WeirdRenderer::Light> g_lights;

		Shader g_flameShader;
		Shader g_particlesShader;
		Shader g_smokeShader;
		Shader g_litShader;

		Mesh* g_monkey = nullptr;
		Mesh* g_quad = nullptr;
		Mesh* g_cube = nullptr;

		Texture* g_noiseTexture0 = nullptr;
		// Texture* g_noiseTexture1 = nullptr;
		Texture* g_flameShape = nullptr;


		Renderer::Renderer(const unsigned int width, const unsigned int height)
			: m_initializer(width, height, m_window)
			, m_windowWidth(width)
			, m_windowHeight(height)
			, m_renderScale(0.5f)
			, m_renderWidth(width * m_renderScale)
			, m_renderHeight(height * m_renderScale)
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



			auto id = g_resourceManager.getMeshId("../assets/monkey/demo.gltf", true);
			g_monkey = &g_resourceManager.getMesh(id);

			///////////////////////////////////////////////////////////
			// Fire
			g_flameShader = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "fire/flame.frag");
			g_particlesShader = Shader(SHADERS_PATH "fire/fireParticles.vert", SHADERS_PATH "fire/fireParticles.frag");
			g_smokeShader = Shader(SHADERS_PATH "fire/smokeParticles.vert", SHADERS_PATH "fire/smokeParticles.frag");
			g_litShader = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "fire/lit.frag");


			g_lights.push_back(
				Light
				{ 
					glm::vec3(0.0f, 1.0f, 0.0f), 
					glm::vec3(0.0f), 
					glm::vec4(1.0f, 0.95f, 0.9f, 5.0f)
				}
			);



			// Quad geom
			{
				float size = 0.5f;
				std::vector<Vertex> vertices = {
					// positions           // normals        // colors         // UVs
					{{-size, -size, 0.f},  {0.f, 0.f, 1.f},  {1.f, 1.f, 1.f}, {0.f, 0.f}}, // bottom left
					{{ size, -size, 0.f},  {0.f, 0.f, 1.f},  {1.f, 1.f, 1.f}, {1.f, 0.f}}, // bottom right
					{{ size,  size, 0.f},  {0.f, 0.f, 1.f},  {1.f, 1.f, 1.f}, {1.f, 1.f}}, // top right
					{{-size,  size, 0.f},  {0.f, 0.f, 1.f},  {1.f, 1.f, 1.f}, {0.f, 1.f}}  // top left
				};

				std::vector<GLuint> indices = {
					0, 2, 1, // first triangle
					3, 2, 0  // second triangle
				};

				std::vector<Texture> textures = {};
				g_quad = new Mesh(1, vertices, indices, textures);
				g_quad->m_isBillboard = true;
			}

			// Cube geom
			{
				float size = 0.5f;
				std::vector<Vertex> vertices = {
					// positions               // normals           // colors         // UVs
					// Front face
					{{-size, -size,  size},  {0.f, 0.f, 1.f},   {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{ size, -size,  size},  {0.f, 0.f, 1.f},   {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{ size,  size,  size},  {0.f, 0.f, 1.f},   {1.f, 1.f, 1.f}, {1.f, 1.f}},
					{{-size,  size,  size},  {0.f, 0.f, 1.f},   {1.f, 1.f, 1.f}, {0.f, 1.f}},

					// Back face
					{{-size, -size, -size},  {0.f, 0.f, -1.f},  {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{ size, -size, -size},  {0.f, 0.f, -1.f},  {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{ size,  size, -size},  {0.f, 0.f, -1.f},  {1.f, 1.f, 1.f}, {0.f, 1.f}},
					{{-size,  size, -size},  {0.f, 0.f, -1.f},  {1.f, 1.f, 1.f}, {1.f, 1.f}},

					// Left face
					{{-size, -size, -size},  {-1.f, 0.f, 0.f},  {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{-size, -size,  size},  {-1.f, 0.f, 0.f},  {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{-size,  size,  size},  {-1.f, 0.f, 0.f},  {1.f, 1.f, 1.f}, {1.f, 1.f}},
					{{-size,  size, -size},  {-1.f, 0.f, 0.f},  {1.f, 1.f, 1.f}, {0.f, 1.f}},

					// Right face
					{{ size, -size,  size},  {1.f, 0.f, 0.f},   {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{ size, -size, -size},  {1.f, 0.f, 0.f},   {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{ size,  size, -size},  {1.f, 0.f, 0.f},   {1.f, 1.f, 1.f}, {1.f, 1.f}},
					{{ size,  size,  size},  {1.f, 0.f, 0.f},   {1.f, 1.f, 1.f}, {0.f, 1.f}},

					// Top face
					{{-size,  size,  size},  {0.f, 1.f, 0.f},   {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{ size,  size,  size},  {0.f, 1.f, 0.f},   {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{ size,  size, -size},  {0.f, 1.f, 0.f},   {1.f, 1.f, 1.f}, {1.f, 1.f}},
					{{-size,  size, -size},  {0.f, 1.f, 0.f},   {1.f, 1.f, 1.f}, {0.f, 1.f}},

					// Bottom face
					{{-size, -size, -size},  {0.f, -1.f, 0.f},  {1.f, 1.f, 1.f}, {0.f, 0.f}},
					{{ size, -size, -size},  {0.f, -1.f, 0.f},  {1.f, 1.f, 1.f}, {1.f, 0.f}},
					{{ size, -size,  size},  {0.f, -1.f, 0.f},  {1.f, 1.f, 1.f}, {1.f, 1.f}},
					{{-size, -size,  size},  {0.f, -1.f, 0.f},  {1.f, 1.f, 1.f}, {0.f, 1.f}},
				};

				std::vector<GLuint> indices = {
					// Front face
					0, 2, 1,  2, 0, 3,
					// Back face
					7, 5, 6,  5, 7, 4,
					// Left face
					11,10,9,  9,8,11,
					// Right face
					12,14,13, 15,14,12,
					// Top face
					19,18,17, 17,16,19,
					// Bottom face
					22,21,20,  20,23,22
				};


				std::vector<Texture> textures = {};
				g_cube = new Mesh(2, vertices, indices, textures);
			}


			// Fire textures
			g_noiseTexture0 = new Texture("../assets/fire.jpg");
			g_flameShape = new Texture("../assets/flame.png");
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


			if (g_fire)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glViewport(0, 0, m_windowWidth, m_windowHeight);

				// Enable depth test
				glEnable(GL_CULL_FACE);
				glEnable(GL_DEPTH_TEST);
				glDepthFunc(GL_LESS);
				glDepthMask(GL_TRUE);


				// Clear
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);



				g_litShader.activate();
				g_litShader.setUniform("u_time", (float)time);
				g_litShader.setUniform("u_ambient", 0.05f);
				

				g_cube->Draw(g_litShader, sceneCamera, vec3(0, -5.0f, 0), vec3(0), vec3(10,10,10), g_lights);
				g_monkey->Draw(g_litShader, sceneCamera, vec3(0, 1, -2.5f), vec3(0, (-3.14f / 2.0f) + time, 0), vec3(1), g_lights);
				g_cube->Draw(g_litShader, sceneCamera, vec3(0.5f, 0.5f, 3), vec3(0, 0.5f, 0), vec3(1), g_lights);

				renderFire(scene, sceneCamera, static_cast<float>(time));

				glDisable(GL_DEPTH_TEST); // No depth test
				glDepthMask(GL_TRUE); // Still write to depth buffer
				glClearDepth(1.0f); // Make sure depth buffer is initialized


				// Swap the back buffer with the front buffer
				glfwSwapBuffers(m_window);
				// Take care of all GLFW events
				glfwPollEvents();

				GL_CHECK_ERROR();

				return;
			}


			// 
			bool enable3D = scene.requires3DRendering();
			bool used2DAsBackground = false;

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
				m_2DsdfShaderProgram.setUniform("u_camMatrix", sceneCamera.view);
				m_2DsdfShaderProgram.setUniform("u_time", scene.getTime());
				m_2DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				m_2DsdfShaderProgram.setUniform("u_blendIterations", 1);

				GLuint u_colorTextureLocation = glGetUniformLocation(m_2DsdfShaderProgram.ID, "t_colorTexture");
				glUniform1i(u_colorTextureLocation, 0);

				m_distanceTexture.bind(0);

				scene.get2DShapesData(m_2DData, m_2DDataSize);
				m_2DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

				m_sdfRenderPlane.Bind();

				m_2DsdfShaderProgram.setUniform("t_shapeBuffer", 1);
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

				GLuint u_colorTextureLocation = glGetUniformLocation(m_postProcessShaderProgram.ID, "t_colorTexture");
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
				glClearColor(0, 0, 0, 1);


				// Draw objects in scene
				scene.renderModels(m_geometryShaderProgram, m_instancedGeometryShaderProgram);

				renderFire(scene, sceneCamera, (float)time);

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
				m_3DsdfShaderProgram.setUniform("u_camMatrix", sceneCamera.view);
				m_3DsdfShaderProgram.setUniform("u_fov", shaderFov);
				m_3DsdfShaderProgram.setUniform("u_time", scene.getTime());
				m_3DsdfShaderProgram.setUniform("u_resolution", glm::vec2(m_renderWidth, m_renderHeight));

				GLuint u_colorTextureLocation = glGetUniformLocation(m_3DsdfShaderProgram.ID, "t_colorTexture");
				glUniform1i(u_colorTextureLocation, 0);
				m_geometryTexture.bind(0);

				GLuint u_depthTextureLocation = glGetUniformLocation(m_3DsdfShaderProgram.ID, "t_depthTexture");
				glUniform1i(u_depthTextureLocation, 1);
				m_geometryDepthTexture.bind(1);

				// Draw render plane with sdf shader
				//scene.renderShapes(m_3DsdfShaderProgram, m_sdfRenderPlane);

				m_3DRenderPlane.Bind();

				m_3DsdfShaderProgram.setUniform("u_loadedObjects", (int)m_2DDataSize);

				m_3DsdfShaderProgram.setUniform("t_shapeBuffer", 2);
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

			GLuint u_colorTextureLocation2d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_2DSceneTexture");
			glUniform1i(u_colorTextureLocation2d, 0);
			if (used2DAsBackground)
			{
				m_lit2DSceneTexture.bind(0);
			}
			else
			{
				m_3DSceneTexture.bind(0);
			}

			GLuint u_colorTextureLocation3d = glGetUniformLocation(m_combineScenesShaderProgram.ID, "t_3DSceneTexture");
			glUniform1i(u_colorTextureLocation3d, 1);
			m_3DSceneTexture.bind(1);

			m_combinationRenderPlane.Bind();
			m_combinationRenderPlane.Draw(m_postProcessShaderProgram);


			m_lit2DSceneTexture.unbind();
			m_3DSceneTexture.unbind();

			output(scene, m_combineResultTexture);
		}

		std::vector<glm::vec3> g_particlesTranslations = {
			vec3(2,0,0),
			vec3(4,0,0),
			vec3(6,0,0),
		};

		std::vector<glm::vec3> g_particlesRotations = {
			vec3(0),
			vec3(0),
			vec3(0),
		};

		std::vector<glm::vec3> g_particlesScales{
			vec3(0.02f),
			vec3(0.02f),
			vec3(0.02f),
		};

		void Renderer::renderFire(Scene& scene, Camera& camera, float time)
		{
			// Particles
			g_particlesShader.activate();
			g_particlesShader.setUniform("u_time", time);
			g_quad->DrawInstances(g_particlesShader, camera,
				10,
				vec3(0, 0.5f, 0),
				vec3(0, 0, time),
				vec3(0.01f),
				g_lights);

			glEnable(GL_BLEND);

			

			// Smoke
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			glDepthMask(GL_FALSE); // Don't write to the depth buffer

			g_smokeShader.activate();
			g_smokeShader.setUniform("u_time", time);
			g_quad->DrawInstances(g_smokeShader, camera,
				50,
				vec3(0, 1.0f, -0.1f),
				vec3(0, 0, time),
				vec3(1.0f),
				g_lights);

			glDepthMask(GL_TRUE);


			// Fire
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			g_flameShader.activate();
			g_flameShader.setUniform("u_time", time);

			g_flameShader.setUniform("t_noise", 0);
			g_noiseTexture0->bind(0);

			g_flameShader.setUniform("t_flameShape", 1);
			g_flameShape->bind(1);

			// Draw flame
			glDisable(GL_CULL_FACE);
			g_quad->Draw(g_flameShader, camera, vec3(0, 2, 0), vec3(0, 0, 0), vec3(4), g_lights);
			glEnable(GL_CULL_FACE);

			g_noiseTexture0->unbind();

			
			glDisable(GL_BLEND);





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

			GLuint u_colorTextureLocation = glGetUniformLocation(m_outputShaderProgram.ID, "t_colorTexture");
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