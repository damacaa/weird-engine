#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>
#include <weird-engine/math/Default3DSDFs.h>

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
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 1, 0);

			MeshRenderer& mr = m_ecs.addComponent<MeshRenderer>(entity);

			auto id = m_resourceManager.getMeshId(ASSETS_PATH "monkey/demo.gltf", entity, true);
			mr.mesh = id;
			// mr.materialIndex = floorMaterial.id;

			m_monkey = entity;
		}

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(2, 3, 2);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = redMat.id;

			m_ball = entity;
		}

		{
			float vars1[8] = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 0.0f}; // Custom shape
			Entity start = addShape(DefaultShapes::STAR, vars1, orangeMat, CombinationType::Addition, true, 0);
		}

		{
			float vars1[8] = {}; // Custom shape
			Entity start = addShape(DefaultShapes3D::PLANE, vars1, floorMaterial, CombinationType::Addition, false);
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
