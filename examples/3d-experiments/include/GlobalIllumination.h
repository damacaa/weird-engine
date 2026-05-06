#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include "DebugPlanes.h"

using namespace WeirdEngine;

// Cornell Box
class GlobalIlluminationScene : public Scene
{
public:
	GlobalIlluminationScene(const PhysicsSettings& settings)
		: Scene(settings) {};

private:

	// Inherited via Scene
	void onStart() override
	{
		m_renderMode = RenderMode::RayMarching3D;
		m_debugFly = true;


		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(-0.5f, -2.0f, 0);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = DisplaySettings::Blue;
		}

    for (size_t i = 0; i < 8; i++)
		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(1.0f + (i), -2.0f, 0);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = (i + 8) % 16;
		}


		{
			std::shared_ptr<IMathExpression> plane = std::make_shared<Plane>(3.0f);
			auto planeId = registerSDF(plane);

			float vars1[8] = {}; // Custom shape
			Entity start = addShape(planeId, vars1, DisplaySettings::Magenta, CombinationType::Addition, false);
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Box>();
			auto boxId = registerSDF(box);

			float vars1[8] = {-5.0f, -2.0f, 0.0f, 0.1f, 1.0f, 3.0f}; // Custom shape
			Entity start = addShape(boxId, vars1, DisplaySettings::Gray, CombinationType::Addition, false);
		}

		getLigths().push_back(
			Light{0, glm::vec3(0.0f, 0.0f, 0.0f), 0, normalize(glm::vec3(0.0f, 0.4f, 1.0f)), glm::vec4(1.0f, 1.0f, 1.0f, 0.5f)});

		// getLigths().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLigths().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		m_ecs.getComponent<Transform>(m_mainCamera).position = vec3(0, 2, 10);
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);

		// getLigths()[0].position.x = cameraTransform.position.x;
		// getLigths()[0].position.y = cameraTransform.position.y;
		// getLigths()[0].position.z = cameraTransform.position.z;

		// getLigths()[0].rotation.x = -cameraTransform.rotation.x;
		// getLigths()[0].rotation.y = -cameraTransform.rotation.y;
		// getLigths()[0].rotation.z = -cameraTransform.rotation.z;
	}
};
