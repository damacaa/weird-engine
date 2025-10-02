#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class FireScene : public Scene
{
private:
	Shader m_flameShader;
	Shader m_particlesShader;
	Shader m_smokeShader;
	Shader m_litShader;
	Shader m_heatDistortionShader;
	Shader m_blurShader;
	Shader m_bloomShader;
	Shader m_brightFilterShader;
	Shader m_backgroundShader;

	Mesh *m_quad = nullptr;
	Mesh *m_cube = nullptr;

	Texture *m_noiseTexture = nullptr;
	Texture *m_flameShape = nullptr;
	Texture *m_sceneTextureBeforeFire = nullptr; // Copy of scene texture for heat effect
	Texture	*m_postProcessTextureFront = nullptr;
	Texture *m_postProcessTextureBack = nullptr;
	Texture *m_brightPassTexture = nullptr;

	RenderTarget *m_postProcessRenderFront;
	RenderTarget *m_postProcessRenderBack;
	RenderTarget *m_bloomRenderTarget;

	RenderTarget *m_postProcessDoubleBuffer[2];

	RenderPlane m_renderPlane;

	std::vector<WeirdRenderer::Light> m_lights;

	void onCreate() override
	{
		m_renderMode = RenderMode::Simple3D;

		// Base shaders
		m_backgroundShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "backgroundGrid.frag");
		m_litShader = Shader(SHADERS_PATH "default.vert", SHADERS_PATH "lit.frag");
		m_bloomShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "bloom.frag");
		m_blurShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "blur.frag");
		m_brightFilterShader = Shader(SHADERS_PATH "renderPlane.vert", SHADERS_PATH "brightFilter.frag");

		// Custom shaders
		m_flameShader = Shader(SHADERS_PATH "default.vert", ASSETS_PATH "fire/shaders/flame.frag");
		m_particlesShader = Shader(ASSETS_PATH "fire/shaders/fireParticles.vert", ASSETS_PATH "fire/shaders/fireParticles.frag");
		m_smokeShader = Shader(ASSETS_PATH "fire/shaders/smokeParticles.vert", ASSETS_PATH "fire/shaders/smokeParticles.frag");
		m_heatDistortionShader = Shader(SHADERS_PATH "default.vert", ASSETS_PATH "fire/shaders/heatDistortion.frag");

		m_lights.push_back(
			Light{
				0,
				glm::vec3(0.0f, 1.0f, 0.0f),
				0,
				glm::vec3(0.0f),
				glm::vec4(1.0f, 0.95f, 0.9f, 2.0f)});

		// Load meshes
		// Quad geom
		{
			float size = 0.5f;
			std::vector<Vertex> vertices = {
				// positions           // normals        // colors         // UVs
				{{-size, -size, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}}, // bottom left
				{{size, -size, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},	 // bottom right
				{{size, size, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},	 // top right
				{{-size, size, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}}	 // top left
			};

			std::vector<GLuint> indices = {
				0, 2, 1, // first triangle
				3, 2, 0	 // second triangle
			};

			std::vector<Texture> textures = {};
			m_quad = new Mesh(1, vertices, indices, textures);
			m_quad->m_isBillboard = true;
		}

		// Cube geom
		{
			float size = 0.5f;
			std::vector<Vertex> vertices = {
				// positions               // normals           // colors         // UVs
				// Front face
				{{-size, -size, size}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{size, -size, size}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{size, size, size}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
				{{-size, size, size}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

				// Back face
				{{-size, -size, -size}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{size, -size, -size}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{size, size, -size}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
				{{-size, size, -size}, {0.f, 0.f, -1.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},

				// Left face
				{{-size, -size, -size}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{-size, -size, size}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{-size, size, size}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
				{{-size, size, -size}, {-1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

				// Right face
				{{size, -size, size}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{size, -size, -size}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{size, size, -size}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
				{{size, size, size}, {1.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

				// Top face
				{{-size, size, size}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{size, size, size}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{size, size, -size}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
				{{-size, size, -size}, {0.f, 1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},

				// Bottom face
				{{-size, -size, -size}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 0.f}},
				{{size, -size, -size}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 0.f}},
				{{size, -size, size}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f}},
				{{-size, -size, size}, {0.f, -1.f, 0.f}, {1.f, 1.f, 1.f}, {0.f, 1.f}},
			};

			std::vector<GLuint> indices = {
				// Front face
				0, 2, 1, 2, 0, 3,
				// Back face
				7, 5, 6, 5, 7, 4,
				// Left face
				11, 10, 9, 9, 8, 11,
				// Right face
				12, 14, 13, 15, 14, 12,
				// Top face
				19, 18, 17, 17, 16, 19,
				// Bottom face
				22, 21, 20, 20, 23, 22};

			std::vector<Texture> textures = {};
			m_cube = new Mesh(2, vertices, indices, textures);
		}

		// Fire textures
		m_noiseTexture = new Texture(ASSETS_PATH "fire/fire.jpg");
		m_flameShape = new Texture(ASSETS_PATH "fire/flame.png");

		m_sceneTextureBeforeFire = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);
		m_postProcessTextureFront = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);
		m_postProcessTextureBack = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);

		m_postProcessRenderFront = new RenderTarget(false);
		m_postProcessRenderFront->bindColorTextureToFrameBuffer(*m_postProcessTextureFront);

		m_postProcessRenderBack = new RenderTarget(false);
		m_postProcessRenderBack->bindColorTextureToFrameBuffer(*m_postProcessTextureBack);

		m_postProcessDoubleBuffer[0] = m_postProcessRenderFront;
		m_postProcessDoubleBuffer[1] = m_postProcessRenderBack;

		// Bloom Texture and Render Target
		m_brightPassTexture = new Texture(Screen::rWidth, Screen::rHeight, Texture::TextureType::Data);
		m_bloomRenderTarget = new RenderTarget(false);
		m_bloomRenderTarget->bindColorTextureToFrameBuffer(*m_brightPassTexture);
	}

	void onDestroy() override
	{
		m_flameShader.free();
		m_particlesShader.free();
		m_smokeShader.free();
		m_litShader.free();
		m_heatDistortionShader.free();
		m_blurShader.free();
		m_bloomShader.free();
		m_brightFilterShader.free();
		m_backgroundShader.free();

		m_quad->free();
		delete m_quad;
		m_cube->free();
		delete m_quad;

		m_noiseTexture->dispose();
		delete m_noiseTexture;
		m_flameShape->dispose();
		delete m_flameShape;
		m_sceneTextureBeforeFire->dispose();
		delete m_sceneTextureBeforeFire;
		m_postProcessTextureFront->dispose();
		delete m_postProcessTextureFront;
		m_postProcessTextureBack->dispose();
		delete m_postProcessTextureBack;
		m_brightPassTexture->dispose();
		delete m_brightPassTexture;

		m_postProcessRenderFront->free();
		delete m_postProcessRenderFront;
		m_postProcessRenderBack->free();
		delete m_postProcessRenderBack;
		m_bloomRenderTarget->free();
		delete m_bloomRenderTarget;

		m_renderPlane.free();
	}

	// Inherited via Scene
	void onStart() override
	{
		m_debugFly = false;
	}

	float m_time = 3.1416f;
	void onUpdate(float delta) override
	{
		if (m_debugFly)
		{
			return;
		}

		if (!Input::GetKey(Input::Space))
		{
			static float speed = 0.15f;
			if (Input::GetKey(Input::R))
			{
				m_time -= delta * speed;
			}
			else
			{
				m_time += delta * speed;
			}
		}

		Transform &cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		static float amplitude = 12.5f;

		cameraTransform.position.y = 2.0f + tan(0.5f * m_time);
		cameraTransform.position.x = amplitude * sin(m_time);
		cameraTransform.position.z = amplitude * cos(m_time);

		cameraTransform.rotation = -cameraTransform.position;
	}

	void renderFire(WeirdRenderer::Camera &camera, float time)
	{
		// Particles
		m_particlesShader.use();
		m_particlesShader.setUniform("u_camMatrix", camera.cameraMatrix);
		m_particlesShader.setUniform("u_camPos", camera.position);
		m_particlesShader.setUniform("u_time", time);
		m_quad->drawInstances(m_particlesShader, camera,
							  10,
							  vec3(0, 1.0f, 0),
							  vec3(0, 0, time),
							  vec3(0.01f));

		glEnable(GL_BLEND);

		// Smoke
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE); // Don't write to the depth buffer

		m_smokeShader.use();
		m_smokeShader.setUniform("u_camMatrix", camera.cameraMatrix);
		m_smokeShader.setUniform("u_camPos", camera.position);
		m_smokeShader.setUniform("u_time", time);
		m_quad->drawInstances(m_smokeShader, camera,
							  50,
							  vec3(0, 1.0f, -0.1f),
							  vec3(0, 0, time),
							  vec3(1.0f));

		glDepthMask(GL_TRUE);

		// Fire
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		m_flameShader.use();
		m_flameShader.setUniform("u_camMatrix", camera.cameraMatrix);
		m_flameShader.setUniform("u_camPos", camera.position);
		m_flameShader.setUniform("u_time", time);

		m_flameShader.setUniform("t_noise", 0);
		m_noiseTexture->bind(0);

		m_flameShader.setUniform("t_flameShape", 1);
		m_flameShape->bind(1);

		// draw flame
		m_quad->draw(m_flameShader, camera, vec3(0, 1.15f, 0), vec3(0, 0, 0), vec3(6));

		m_noiseTexture->unbind();

		glDisable(GL_BLEND);
	}

	void onRender(WeirdRenderer::RenderTarget &renderTarget) override
	{

		WeirdRenderer::Camera &sceneCamera = getCamera();
		float time = getTime();

		glDepthMask(GL_FALSE);
		m_backgroundShader.use();
		m_backgroundShader.setUniform("u_camMatrix", sceneCamera.cameraMatrix);
		float shaderFov = 1.0f / tan(sceneCamera.fov * 0.01745f * 0.5f);
		m_backgroundShader.setUniform("u_fov", shaderFov);
		m_backgroundShader.setUniform("u_resolution", vec2(Screen::rWidth, Screen::rHeight));

		m_renderPlane.draw(m_backgroundShader);
		// glFrontFace(GL_CW); // Clockwise = front face
		// m_cube->draw(m_backgroundShader, sceneCamera, sceneCamera.position, vec3(0.0f), vec3(100.0f));
		// glFrontFace(GL_CCW); // Counter-clockwise = front face (default)

		glDepthMask(GL_TRUE);

		GL_CHECK_ERROR();

		// Render stuff
		m_litShader.use();
		m_litShader.setUniform("u_time", (float)time);
		m_litShader.setUniform("u_ambient", 0.025f);

		// Take care of the camera Matrix
		m_litShader.setUniform("u_camPos", sceneCamera.position);
		m_litShader.setUniform("u_camMatrix", sceneCamera.cameraMatrix);

		// Pass light rotation
		glm::vec3 position = m_lights[0].position;
		m_litShader.setUniform("u_lightPos", position);
		glm::vec3 direction = m_lights[0].rotation;
		m_litShader.setUniform("u_directionalLightDir", direction);
		glm::vec4 color = m_lights[0].color;
		m_litShader.setUniform("u_lightColor", color);

		// bind current FBO
		// m_sceneRender->bind(); // TODO: keep rendering to the same fbo. Should pass the render target and the color and depth textures

		// Floor
		m_cube->draw(m_litShader, sceneCamera, vec3(0, -500.0f, 0), vec3(0), vec3(4, 1000, 4));

		m_cube->draw(m_litShader, sceneCamera, vec3(0.2f, 1.75f, -2.5f), vec3(-3.14f / 4.0f), vec3(1));

		// m_monkey->draw(m_litShader, sceneCamera, vec3(0, 1, -2.5f), vec3(0, (-3.14f / 2.0f), 0), vec3(1));
		m_cube->draw(m_litShader, sceneCamera, vec3(0.8f, 1.0f, 2.5f), vec3(0.5f, 0.5f, 0), vec3(1));
		// m_cube->draw(m_litShader, sceneCamera, vec3(0.3f, 0.5f, 3.2f), vec3(0, 1.2f, 0), vec3(1));
		// m_cube->draw(m_litShader, sceneCamera, vec3(0.9f, 1.5f, 2.9f), vec3(0, 0.75f, 0), vec3(1));

		m_cube->draw(m_litShader, sceneCamera, vec3(-2.0f, 2.75f, 0.9f), vec3(-0.75f, 0.0f, 0), vec3(1));


		GL_CHECK_ERROR();

		// m_cube->draw(m_litShader, sceneCamera, vec3(0, 0.0f, 0), vec3(0), vec3(0.1f, 2.0f, 0.1f));

		// Heat effect

		// Don't write to depth buffer
		glDepthMask(GL_FALSE);

		// Copy scene texture // TODO: replace with glCopyTexSubImage which copies from the current read framebuffer attachment to the given image
		glCopyImageSubData(
			renderTarget.getColorAttachment()->ID, GL_TEXTURE_2D, 0, 0, 0, 0, // 2 = scene texture
			m_sceneTextureBeforeFire->ID, GL_TEXTURE_2D, 0, 0, 0, 0,
			Screen::rWidth, Screen::rHeight, 1);

		// Heat effect shader
		m_heatDistortionShader.use();
		m_heatDistortionShader.setUniform("u_camPos", sceneCamera.position);
		m_heatDistortionShader.setUniform("u_camMatrix", sceneCamera.cameraMatrix);

		// Takes copy texture
		m_heatDistortionShader.setUniform("t_texture", 0);
		m_sceneTextureBeforeFire->bind(0);
		// Noise texture
		m_heatDistortionShader.setUniform("t_noise", 1);
		m_noiseTexture->bind(1);
		// Flame texture
		m_heatDistortionShader.setUniform("t_flameShape", 2);
		m_flameShape->bind(2);

		GL_CHECK_ERROR();

		// Time to animate effect
		m_heatDistortionShader.setUniform("u_time", (float)time);
		// Screen resolution to calculate screen uvs
		m_heatDistortionShader.setUniform("u_resolution", glm::vec2(Screen::rWidth, Screen::rHeight));

		// Render quad
		m_quad->draw(m_heatDistortionShader, sceneCamera, vec3(0, 1.25f, 0), vec3(0, 0, 0), vec3(6));

		// Write to depth buffer
		glDepthMask(GL_TRUE);

		// Fire
		renderFire(sceneCamera, time);

		if (Input::GetKey(Input::P))
		{
			return;
		}

		// Post processing
		m_bloomRenderTarget->bind();
		m_brightFilterShader.use();
		m_brightFilterShader.setUniform("t_colorTexture", 0);
		renderTarget.getColorAttachment()->bind(0);

		m_renderPlane.draw(m_brightFilterShader);

		m_blurShader.use();
		m_blurShader.setUniform("t_colorTexture", 0);

		bool horizontal = true;
		static int amount = 10;

		for (unsigned int i = 0; i < amount; i++)
		{
			m_postProcessDoubleBuffer[horizontal]->bind();

			m_blurShader.setUniform("u_horizontal", horizontal);
			if (i == 0)
			{
				m_brightPassTexture->bind(0);
			}
			else
			{
				m_postProcessDoubleBuffer[!horizontal]->getColorAttachment()->bind(0);
			}

			m_renderPlane.draw(m_blurShader);

			horizontal = !horizontal;
		}

		// bind a different FBO
		renderTarget.bind(); // bind to fbo 0

		glDisable(GL_DEPTH_TEST);

		// Use scene texture and a shader to apply pp
		m_bloomShader.use();
		m_bloomShader.setUniform("t_sceneTexture", 0);
		renderTarget.getColorAttachment()->bind(0);

		m_bloomShader.setUniform("t_blurTexture", 1);
		RenderTarget *finalTarget = m_postProcessDoubleBuffer[!horizontal];
		finalTarget->getColorAttachment()->bind(1);

		if (Input::GetKey(Input::B))
		{
			finalTarget->getColorAttachment()->bind(0);
		}

		m_bloomShader.setUniform("u_pixelOffset", vec2(0.5f / Screen::rWidth, 0.5f / Screen::rHeight));

		m_renderPlane.draw(m_bloomShader);
	}
};

class FireSceneRayMarching : public FireScene
{
private:
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;

		Transform& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		cameraTransform.position = vec3(0.0f, 1.0f, 10.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}
};