#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class LinesScene : public Scene
{
public:
	LinesScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:
	Texture* m_colorTextureCopy;

	RenderPlane* m_renderPlane;
	Shader* m_lineShader;

	RenderTarget* m_lineRender;
	Texture* m_lineTexture;

	Shader* m_combinationShader;

	Entity m_monkey;

	void onCreate() override
	{
		m_renderPlane = new RenderPlane();
		m_colorTextureCopy = new Texture(Display::rWidth, Display::rHeight, Texture::TextureType::Data);
		m_lineShader = new Shader(SHADERS_PATH "common/screen_plane.vert", ASSETS_PATH "lines/lines.frag");

		m_lineRender = new RenderTarget(false);
		m_lineTexture = new Texture(Display::rWidth, Display::rHeight, Texture::TextureType::Data);
		m_lineRender->bindColorTextureToFrameBuffer(*m_lineTexture);

		m_combinationShader =
			new Shader(SHADERS_PATH "common/screen_plane.vert", ASSETS_PATH "lines/combination.frag");
	}

	// Inherited via Scene
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;
		m_debugFly = false;
		getLigths().push_back(Light{});

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 0, 0);

			// MeshRenderer &mr = m_ecs.addComponent<MeshRenderer>(entity);

			// auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
			// mr.mesh = id;

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = 0;

			m_monkey = entity;
		}
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		Transform& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
		cameraTransform.position.y = 5.0f;
		cameraTransform.position.z -= 10.0f * delta;

		Transform& monkeyTransform = m_ecs.getComponent<Transform>(m_monkey);
		monkeyTransform.position = cameraTransform.position;
		monkeyTransform.position.z -= 5.0f;
	}

	void onRender(WeirdRenderer::RenderTarget& renderTarget) override
	{
		m_lineRender->bind();
		glClearColor(0, 0, 0, 0);							// Set clear color
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear both buffers

		// GLES 3.0-compatible copy: source color attachment -> destination texture.
		GLint prevReadFbo = 0;
		GLint prevReadBuffer = 0;
		GLint prevTexture2D = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prevReadFbo);
		glGetIntegerv(GL_READ_BUFFER, &prevReadBuffer);
		glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture2D);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, renderTarget.getFrameBuffer());
		glReadBuffer(GL_COLOR_ATTACHMENT0);

		glBindTexture(GL_TEXTURE_2D, m_colorTextureCopy->ID);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, Display::rWidth, Display::rHeight);

		glBindTexture(GL_TEXTURE_2D, (GLuint)prevTexture2D);
		glReadBuffer(prevReadBuffer);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prevReadFbo);

		// --- Line detection ---
		m_lineShader->use();

		m_lineShader->setUniform("t_sceneDepth", 0);
		renderTarget.getDepthAttachment()->bind(0);

		m_lineShader->setUniform("u_pixelSize", vec2(1.0f / Display::rWidth, 1.0f / Display::rHeight));

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_BLEND);
		m_renderPlane->draw(*m_lineShader);
		glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);

		// --- Final draw ---
		renderTarget.bind();

		m_combinationShader->use();

		m_combinationShader->setUniform("t_scene", 0);
		m_colorTextureCopy->bind(0);

		m_combinationShader->setUniform("t_lines", 1);
		m_lineTexture->bind(1);

		glDisable(GL_DEPTH_TEST);
		// glEnable(GL_BLEND);
		m_renderPlane->draw(*m_combinationShader);
		// glDisable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
	}
};