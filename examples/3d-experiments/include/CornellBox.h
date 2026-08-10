#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include <weird-engine/math/Default3DSDFs.h>

using namespace WeirdEngine;

// Cornell Box
class CornellBox : public Scene3D
{
public:
	CornellBox() {};

private:
	Entity m_sunLight;
	// Inherited via Scene
	void onStart(ECSManager& ecs, ServiceProvider& services) override
	{
		services.debug().setDebugFly(true);

		auto& ballMat = services.materials().createMaterial();
		ballMat.color = vec4(1.0f);
		ballMat.metallic = 1.0f;
		ballMat.roughness = 0.005f;

		auto& redMat = services.materials().createMaterial();
		redMat.color = vec4(.8f, 0.2f, 0.2f, 1.0f);

		auto& greenMat = services.materials().createMaterial();
		greenMat.color = vec4(0.1f, .95f, 0.1f, 1.0f);
		greenMat.metallic = 0.5f;
		greenMat.roughness = 0.1f;

		auto& whiteMat = services.materials().createMaterial();
		whiteMat.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(0.0f, 0.75f, 0.0f);

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = ballMat.id;
		}

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(20.0f, 20.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(entity);
			text.text = "Cornell Box";
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			// Left
			{
				float vars1[8] = {-2.0f * 2.6f, 2.6f, 0.0f, 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, redMat, CombinationType::Addition, false);
			}

			// Right
			{
				float vars1[8] = {2.0f * 2.6f, 2.6f, 0.0f, 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, greenMat, CombinationType::Addition, false);
			}

			// Back
			{
				float vars1[8] = {0.0f, 2.6f, -2.0f * 2.6f, 3.0f * 2.6f, 2.6f, 2.6f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, whiteMat, CombinationType::Addition, false);
			}

			// Top
			{
				float vars1[8] = {0.0f, 3.0f * 2.6f, -2.6f, 3.0f * 2.6f, 2.6f, 2.0f * 2.6f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, whiteMat, CombinationType::Addition, false);
			}

			// Light hole
			{
				float vars1[8] = {0.0f, 2.0f * 2.6f, 0.0f, 0.5f, 1.0f, 0.5f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, whiteMat, CombinationType::Subtraction, false);
			}

			// Floor
			{
				float vars1[8] = {0.0f, -1.0f * 2.6f, -2.6f, 3.0f * 2.6f, 2.6f, 2.0f * 2.6f}; // Custom shape
				Entity start = services.shapes().addShape(boxId, vars1, whiteMat, CombinationType::Addition, false);
			}

			// {
			// 	float vars1[8] = {0.0f, 2.6f, 0.0f, 2.7f, 2.7f, 2.7f}; // Custom shape
			// 	Entity start = services.shapes().addShape(boxId, vars1, DisplaySettings::White,
			// CombinationType::Intersection, false);
			// }
		}

		// Sun
		{
			m_sunLight = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(m_sunLight);
			t.position = glm::vec3(0.0f, 0.0f, 0.0f);
			t.rotation = normalize(glm::vec3(0.0f, 0.0f, 0.0f));

			LightComponent& lc = ecs.addComponent<LightComponent>(m_sunLight);
			lc.type = LightType::Directional;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		}

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = glm::vec3(0.0f, (2.0f * 2.6f) + 0.25f, 0.0f);
			t.rotation = glm::vec3(0.0f, 0.0f, 0.0f);

			LightComponent& lc = ecs.addComponent<LightComponent>(entity);
			lc.type = LightType::Point;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f);
		}

		// getLights().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLights().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		ecs.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(0, 2.6f, 12.0f);
	}

	void onUpdate(ECSManager& ecs, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q))
		{
			services.sceneControl().goToNextScene();
		}

		auto& cameraTransform = ecs.getComponent<Transform>(services.render().getCameraEntity());

		auto& lightTransform = ecs.getComponent<Transform>(m_sunLight);
		lightTransform.position = cameraTransform.position;
		lightTransform.rotation = -cameraTransform.rotation;
	}
};
