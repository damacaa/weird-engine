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
	void onStart(Registry& registry, ServiceProvider& services) override
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
			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(0.0f, 0.75f, 0.0f);

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = ballMat.id;
		}

		{
			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(20.0f, 20.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(entity);
			text.text = "Cornell Box";
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			// Left
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, -2.0f * 2.6f},
													  {Primitives3D::Box::POS_Y, 2.6f},
													  {Primitives3D::Box::POS_Z, 0.0f},
													  {Primitives3D::Box::SIZE_X, 2.6f},
													  {Primitives3D::Box::SIZE_Y, 2.6f},
													  {Primitives3D::Box::SIZE_Z, 2.6f}},
										.material = redMat,
										.combination = CombinationType::Addition,
										.hasCollision = false});

			// Right
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 2.0f * 2.6f},
													  {Primitives3D::Box::POS_Y, 2.6f},
													  {Primitives3D::Box::POS_Z, 0.0f},
													  {Primitives3D::Box::SIZE_X, 2.6f},
													  {Primitives3D::Box::SIZE_Y, 2.6f},
													  {Primitives3D::Box::SIZE_Z, 2.6f}},
										.material = greenMat,
										.combination = CombinationType::Addition,
										.hasCollision = false});

			// Back
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 0.0f},
													  {Primitives3D::Box::POS_Y, 2.6f},
													  {Primitives3D::Box::POS_Z, -2.0f * 2.6f},
													  {Primitives3D::Box::SIZE_X, 3.0f * 2.6f},
													  {Primitives3D::Box::SIZE_Y, 2.6f},
													  {Primitives3D::Box::SIZE_Z, 2.6f}},
										.material = whiteMat,
										.combination = CombinationType::Addition,
										.hasCollision = false});

			// Top
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 0.0f},
													  {Primitives3D::Box::POS_Y, 3.0f * 2.6f},
													  {Primitives3D::Box::POS_Z, -2.6f},
													  {Primitives3D::Box::SIZE_X, 3.0f * 2.6f},
													  {Primitives3D::Box::SIZE_Y, 2.6f},
													  {Primitives3D::Box::SIZE_Z, 2.0f * 2.6f}},
										.material = whiteMat,
										.combination = CombinationType::Addition,
										.hasCollision = false});

			// Light hole
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 0.0f},
													  {Primitives3D::Box::POS_Y, 2.0f * 2.6f},
													  {Primitives3D::Box::POS_Z, 0.0f},
													  {Primitives3D::Box::SIZE_X, 0.5f},
													  {Primitives3D::Box::SIZE_Y, 1.0f},
													  {Primitives3D::Box::SIZE_Z, 0.5f}},
										.material = whiteMat,
										.combination = CombinationType::Subtraction,
										.hasCollision = false});

			// Floor
			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 0.0f},
													  {Primitives3D::Box::POS_Y, -1.0f * 2.6f},
													  {Primitives3D::Box::POS_Z, -2.6f},
													  {Primitives3D::Box::SIZE_X, 3.0f * 2.6f},
													  {Primitives3D::Box::SIZE_Y, 2.6f},
													  {Primitives3D::Box::SIZE_Z, 2.0f * 2.6f}},
										.material = whiteMat,
										.combination = CombinationType::Addition,
										.hasCollision = false});

			// {
			// 	float vars1[8] = {0.0f, 2.6f, 0.0f, 2.7f, 2.7f, 2.7f}; // Custom shape
			// 	Entity start = services.shapes().addShape(boxId, vars1, DisplaySettings::White,
			// CombinationType::Intersection, false);
			// }
		}

		// Sun
		{
			m_sunLight = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(m_sunLight);
			t.position = glm::vec3(0.0f, 0.0f, 0.0f);
			t.rotation = normalize(glm::vec3(0.0f, 0.0f, 0.0f));

			LightComponent& lc = registry.addComponent<LightComponent>(m_sunLight);
			lc.type = LightType::Directional;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.0f);
		}

		{
			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = glm::vec3(0.0f, (2.0f * 2.6f) + 0.25f, 0.0f);
			t.rotation = glm::vec3(0.0f, 0.0f, 0.0f);

			LightComponent& lc = registry.addComponent<LightComponent>(entity);
			lc.type = LightType::Point;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 3.0f);
		}

		// getLights().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLights().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(0, 2.6f, 12.0f);
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q))
		{
			services.sceneControl().goToNextScene();
		}

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());

		auto& lightTransform = registry.getComponent<Transform>(m_sunLight);
		lightTransform.position = cameraTransform.position;
		lightTransform.rotation = -cameraTransform.rotation;
	}
};
