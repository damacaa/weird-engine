#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include "DebugPlanes.h"

using namespace WeirdEngine;
class ClassicScene : public Scene
{
public:
	ClassicScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:
	Entity m_monkey;
	Entity m_ball;

	// Inherited via Scene
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;
		m_debugFly = true;

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 1, 0);

			MeshRenderer& mr = m_ecs.addComponent<MeshRenderer>(entity);

			auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
			mr.mesh = id;

			m_monkey = entity;
		}

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(2, 3, 2);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = DisplaySettings::Red;

			m_ball = entity;
		}

		{
			float vars1[8] = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 0.0f}; // Custom shape
			Entity start = addShape(DefaultShapes::STAR, vars1, DisplaySettings::Orange, CombinationType::Addition, true, 0);
		}

		{
			std::shared_ptr<IMathExpression> plane = std::make_shared<Plane>(0.0f);
			auto planeId = registerSDF(plane);

			float vars1[8] = {}; // Custom shape
			Entity start = addShape(planeId, vars1, 0);
		}

		getLigths().push_back(
			Light{0, glm::vec3(0.0f, 3.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(1.0f, 0.95f, 0.9f, 2.0f)});

		m_ecs.getComponent<Transform>(m_mainCamera).position = vec3(0, 2, 10);
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		Transform& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		return;

		{
			Transform& t = m_ecs.getComponent<Transform>(m_monkey);
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
