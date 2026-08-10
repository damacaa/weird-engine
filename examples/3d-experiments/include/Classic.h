#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>
#include <weird-engine/math/Default3DSDFs.h>

using namespace WeirdEngine;
class ClassicScene : public Scene3D
{
private:
	Entity m_monkey;
	Entity m_ball;

	// Inherited via Scene
	void onStart(ECSManager& ecs, ServiceProvider& services) override
	{
		services.debug().setDebugFly(true);

		auto& redMat = createMaterial();
		redMat.color = vec4(.8f, 0.2f, 0.2f, 1.0f);

		auto& orangeMat = createMaterial();
		orangeMat.color = vec4(.95f, 0.4f, 0.1f, 1.0f);

		auto& floorMaterial = createMaterial();
		floorMaterial.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		floorMaterial.secondaryColor = vec4(0.4f, 0.4f, 0.6f, 1.0f);
		floorMaterial.metallic = 0.7f;
		floorMaterial.roughness = 0.1f;
		floorMaterial.pattern = MaterialPattern::Checkers;

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 1, 0);

			MeshRenderer& mr = ecs.addComponent<MeshRenderer>(entity);

			auto id = services.resources().getMeshId("monkey/demo.gltf", entity, true);
			mr.mesh = id;
			// mr.materialIndex = floorMaterial.id;

			m_monkey = entity;
		}

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(2, 3, 2);

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = redMat.id;

			m_ball = entity;
		}

		{
			float vars1[8] = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 0.0f}; // Custom shape
			Entity start =
				services.shapes().addShape(DefaultShapes::STAR, vars1, orangeMat, CombinationType::Addition, true, 0);
		}

		{
			float vars1[8] = {}; // Custom shape
			Entity start = services.shapes().addShape(DefaultShapes3D::PLANE, vars1, floorMaterial,
													  CombinationType::Addition, false);
		}

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = glm::vec3(0.0f, 3.0f, 0.0f);
			t.rotation = glm::vec3(0.35f, 0.45f, 0.5f);

			LightComponent& lc = ecs.addComponent<LightComponent>(entity);
			lc.type = LightType::Directional;
			lc.color = glm::vec4(1.0f, 0.95f, 0.9f, 2.0f);
		}

		ecs.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(0, 2, 10);
	}

	void onUpdate(ECSManager& ecs, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q))
		{
			services.sceneControl().goToNextScene();
		}

		Transform& cameraTransform = ecs.getComponent<Transform>(services.render().getCameraEntity());

		return;

		{
			Transform& t = ecs.getComponent<Transform>(m_monkey);
		}

		{
			Transform& t = ecs.getComponent<Transform>(m_ball);
			// t.position.z = 10 * sinf(getTime());
			t.position.x = 2.0f * sinf(-getTime());
			t.position.z = 2.0f * cosf(-getTime());
		}

		// t.position = cameraTransform.position + vec3(-10, -6, -20);
	}
};
