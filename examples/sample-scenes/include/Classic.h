#pragma once 

#include <weird-engine.h>

using namespace WeirdEngine;
class ClassicScene : public Scene
{
private:

	Entity m_monkey;
	Entity m_ball;

	// Inherited via Scene
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0,0,0);

			MeshRenderer& mr = m_ecs.addComponent<MeshRenderer>(entity);

			auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
			mr.mesh = id;

			m_monkey = entity;
		}

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 1, 0);

			auto& sdf = m_ecs.addComponent<SDFRenderer>(entity);
			sdf.materialId = 0;

			m_ball = entity;
		}
	}

	void onUpdate(float delta) override
	{
		Transform& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		{
			Transform& t = m_ecs.getComponent<Transform>(m_monkey);
			// t.position.z = 10 * sinf(getTime());
			t.rotation.y = getTime();
			t.position.y = 1.0f;
		}

		{
			Transform& t = m_ecs.getComponent<Transform>(m_ball);
			// t.position.z = 10 * sinf(getTime());
			t.position.x = 2.0f * sinf(-getTime());
			t.position.z = 2.0f * cosf(-getTime());
			
		}

		// t.position = cameraTransform.position + vec3(-10, -6, -20);
	}
};