#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include "DebugPlanes.h"

using namespace WeirdEngine;

// Cornell Box
class CornellBox : public Scene
{
public:
	CornellBox(const PhysicsSettings& settings)
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
			t.position = vec3(0.0f, 0.75f, 0.0f);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = DisplaySettings::White;
		}




		// {
		// 	std::shared_ptr<IMathExpression> plane = std::make_shared<Plane>(0.0f);
		// 	auto planeId = registerSDF(plane);

		// 	float vars1[8] = {}; // Custom shape
		// 	Entity start = addShape(planeId, vars1, DisplaySettings::White, CombinationType::Addition, false);
		// }

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Box>();
			auto boxId = registerSDF(box);

      // Left
			{
				float vars1[8] = {-2.0f * 2.6f, 2.6f, 0.0f, 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::Red, CombinationType::Addition, false);
			}

      // Right
      {
				float vars1[8] = {2.0f * 2.6f, 2.6f, 0.0f, 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::Green, CombinationType::Addition, false);
			}

      // Back
      {
				float vars1[8] = {0.0f, 2.6f, -2.0f * 2.6f, 3.0f * 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::White, CombinationType::Addition, false);
			}

      // Top
      {
				float vars1[8] = {0.0f, 3.0f * 2.6f, -2.6f, 3.0f * 2.6f, 2.6f, 2.0f * 2.6f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::White, CombinationType::Addition, false);
			}

      // Light hole
      {
				float vars1[8] = {0.0f, 2.0f * 2.6f, 0.0f, 0.5f, 1.0f, 0.5f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::White, CombinationType::Subtraction, false);
			}

      // Floor
      {
				float vars1[8] = {0.0f, -1.0f * 2.6f, -2.6f, 3.0f * 2.6f, 2.6f, 2.0f * 2.6f}; // Custom shape
				Entity start = addShape(boxId, vars1, DisplaySettings::White, CombinationType::Addition, false);
			}

      // {
			// 	float vars1[8] = {0.0f, 2.6f, 0.0f, 2.7f, 2.7f, 2.7f}; // Custom shape
			// 	Entity start = addShape(boxId, vars1, DisplaySettings::White, CombinationType::Intersection, false);
			// }

		}
    
    // Sun
    getLigths().push_back(
			Light{0, glm::vec3(0.0f, 0.0f, 0.0f), 0, normalize(glm::vec3(0.0f, 0.0f, 0.0f)), glm::vec4(1.0f, 1.0f, 1.0f, 0.0f)});

		getLigths().push_back(
			Light{1, glm::vec3(0.0f, (2.0f * 2.6f) + 0.25f, 0.0f), 0, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec4(1.0f, 1.0f, 1.0f, 3.0f)});

		// getLigths().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLigths().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		m_ecs.getComponent<Transform>(m_mainCamera).position = vec3(0, 2.6f, 12.0f);
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
