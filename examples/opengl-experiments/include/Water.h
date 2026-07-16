#pragma once

#include <weird-engine.h>
#include "WaterPlane.h"

using namespace WeirdEngine;

class WaterScene : public Scene3D
{
public:
	WaterScene(){};

private:
	Shader m_waterShader;

	RenderPlane m_renderPlane;

	// Scene snapshot – taken each frame before the water draw to avoid
	// reading from and writing to the same framebuffer attachment.
	RenderTarget m_snapshotRender;
	Texture      m_snapshotColor;
	Texture      m_snapshotDepth;
	int          m_snapshotW = 0;
	int          m_snapshotH = 0;

	WaterPlane m_waterPlane;

	void rebuildSnapshot(int w, int h)
	{
		if (m_snapshotW != 0)
		{
			m_snapshotRender.free();
			m_snapshotColor.dispose();
			m_snapshotDepth.dispose();
		}
		m_snapshotColor  = Texture(w, h, Texture::TextureType::Data);
		m_snapshotDepth  = Texture(w, h, Texture::TextureType::Depth);
		m_snapshotRender = RenderTarget(false);
		m_snapshotRender.bindColorTextureToFrameBuffer(m_snapshotColor);
		m_snapshotRender.bindDepthTextureToFrameBuffer(m_snapshotDepth);
		m_snapshotW = w;
		m_snapshotH = h;
	}


	// -------------------------------------------------------------------------

	void onCreate() override
	{

		m_waterShader = Shader(ASSETS_PATH "water/shaders/water.vert",
		                       ASSETS_PATH "water/shaders/water.frag");

		getLigths().push_back(Light{0, glm::vec3(0.0f, 0.0f, 0.0f), 0, normalize(glm::vec3(0.0f, 0.4f, 1.0f)),
									glm::vec4(1.0f, 1.0f, 1.0f, 0.5f)});

		m_waterPlane.build();
	}

	void onDestroy() override
	{
		m_waterShader.free();

		m_renderPlane.free();

		if (m_snapshotW != 0)
		{
			m_snapshotRender.free();
			m_snapshotColor.dispose();
			m_snapshotDepth.dispose();
		}
	}

	Entity m_dot;

	struct Floatable
	{
		float buoyancy = 10.0f; // how strongly this entity is affected by the water surface
	};

	void onStart(ECSManager& ecs) override
	{
		m_debugFly = true;

		auto& redMat = createMaterial();
		redMat.color = vec4(.8f, 0.2f, 0.2f, 1.0f);

		{
			m_dot = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(m_dot);
			t.position = vec3(0, 0, 0);
			auto& renderer = ecs.addComponent<Dot>(m_dot);
			renderer.materialId = redMat.id;
			ecs.addComponent<Floatable>(m_dot);
		}

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(3, 0, 0);

			MeshRenderer& mr = ecs.addComponent<MeshRenderer>(entity);
			auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
			mr.mesh = id;
			
			ecs.addComponent<Floatable>(entity);
		}

		ecs.getComponent<Transform>(m_mainCamera).position = vec3(0, 3, 20);
	}

	float m_time = 0.0f;

	void onUpdate(float delta, ECSManager& ecs) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		m_time += delta;

		
		const auto& floatables = ecs.getComponentArray<Floatable>();

		for(int i = 0; i < floatables->getSize(); i++)
		{
			auto& floatable = floatables->getDataAtIdx(i);

			// Keep the dot riding the water surface
			Transform& transform = ecs.getComponent<Transform>(floatables->getEntityAtIdx(i));
			glm::vec2 flatPos = { transform.position.x, transform.position.z };
			float centerHeight = m_waterPlane.waterHeightAt(flatPos, m_time);
			transform.position.y = centerHeight;

			// Derive surface normal via central finite difference, then drift along it
			const float fdStep = 0.05f;
			float heightPlusX = m_waterPlane.waterHeightAt(flatPos + glm::vec2(fdStep, 0.0f), m_time);
			float heightPlusZ = m_waterPlane.waterHeightAt(flatPos + glm::vec2(0.0f,  fdStep), m_time);

			glm::vec3 surfaceNormal = glm::normalize(glm::vec3(heightPlusX - centerHeight, fdStep, heightPlusZ - centerHeight));
			transform.position.x += surfaceNormal.x * delta * floatable.buoyancy;
			transform.position.z += surfaceNormal.z * delta * floatable.buoyancy;
		}

		

	}

	void onRender(WeirdRenderer::RenderTarget& renderTarget) override
	{
		WeirdRenderer::Camera& sceneCamera = getCamera();
		float time = getTime();

		auto& lights = getLigths();


		// ── Snapshot the current scene colour + depth ────────────────────────
		// We need to read from these textures while drawing the water plane,
		// so we must copy them to separate textures first to avoid a feedback loop.
		const Texture* colorAttach = renderTarget.getColorAttachment(0);
		int w = colorAttach->getWidth();
		int h = colorAttach->getHeight();

		if (w != m_snapshotW || h != m_snapshotH)
			rebuildSnapshot(w, h);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, renderTarget.getFrameBuffer());
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_snapshotRender.getFrameBuffer());
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

		// Restore the original render target so the water draws into it
		renderTarget.bind();

		// ── Water plane ─────────────────────────────────────────────────────
		// Alpha-blending is handled manually in the shader; draw fully opaque.
		glDisable(GL_BLEND);

		m_waterShader.use();
		m_waterShader.setUniform("u_time",      time);
		m_waterShader.setUniform("u_camPos",    sceneCamera.position);
		m_waterShader.setUniform("u_camMatrix", sceneCamera.cameraMatrix);
		m_waterShader.setUniform("u_near",      sceneCamera.nearPlane);
		m_waterShader.setUniform("u_far",       sceneCamera.farPlane);

		// Scene snapshot textures for underwater effects
		m_waterShader.setUniform("u_sceneColor",    0);
		m_snapshotColor.bind(0);
		m_waterShader.setUniform("u_sceneDepth",    1);
		m_snapshotDepth.bind(1);
		m_waterShader.setUniform("u_screenSize",    glm::vec2((float)w, (float)h));

		int numLights = (std::min)((int)lights.size(), 8);
		m_waterShader.setUniform("u_numLights", numLights);
		for (int i = 0; i < numLights; i++)
		{
			std::string prefix = "u_lights[" + std::to_string(i) + "].";
			m_waterShader.setUniform(prefix + "position",  lights[i].position);
			m_waterShader.setUniform(prefix + "direction", lights[i].rotation);
			m_waterShader.setUniform(prefix + "color",     lights[i].color);
			m_waterShader.setUniform(prefix + "type",      (int)lights[i].type);
		}

		glm::mat4 waterModel = glm::mat4(1.0f);
		m_waterPlane.draw(m_waterShader, waterModel);
	}
};
