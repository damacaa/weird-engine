#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include <weird-engine/math/Default3DSDFs.h>

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

		auto& floorMaterial = createMaterial();
		floorMaterial.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		floorMaterial.metallic = 0.0f;
		floorMaterial.roughness = 0.3f;
		floorMaterial.pattern = MaterialPattern::Checkers;

		auto& mirrorMaterial = createMaterial();
		mirrorMaterial.color = vec4(1.0f);
		mirrorMaterial.metallic = 1.0f;
		mirrorMaterial.roughness = 0.0f;
		
		std::vector<uint16_t> randomMats;
		vec4 colors[] = {
			vec4(.95f, 0.4f, 0.1f, 1.0f),       // Orange
			vec4(0.5f, 0.0f, 1.0f, 1.0f),       // Purple
			vec4(0.0f, .9f, .9f, 1.0f),         // Cyan
			vec4(1.0f, 0.3f, .6f, 1.0f),        // Magenta
			vec4(0.5f, 1.0f, 0.5f, 1.0f),       // Light Green
			vec4(1.0f, 0.5f, 0.5f, 1.0f),       // Pink
			vec4(0.5f, 0.5f, 1.0f, 1.0f),       // Light Blue
			vec4(0.4f, 0.25f, 0.1f, 1.0f)       // Brown
		};

		for (int i = 0; i < 8; i++) {
			auto& mat = createMaterial();
			mat.color = colors[i];
			randomMats.push_back(mat.id);
		}

		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(-0.5f, -2.0f, 0);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = randomMats[rand() % randomMats.size()];
		}

    	for (size_t i = 0; i < 8; i++)
		{
			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(1.0f + (i), -2.0f, 0);

			auto& sdf = m_ecs.addComponent<Dot>(entity);
			sdf.materialId = randomMats[i];
		}

		{
			float vars[8] = {3};
			Entity floor = addShape(DefaultShapes3D::PLANE, vars, floorMaterial, CombinationType::Addition, false);
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = registerSDF(box);

			float vars1[8] = {-5.0f, -2.0f, 0.0f, 0.1f, 1.0f, 3.0f}; // Custom shape
			Entity start = addShape(boxId, vars1, mirrorMaterial, CombinationType::Addition, false);
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = registerSDF(box);

			float vars1[8] = {20.0f, -2.0f, 0.0f, 0.1f, 1.0f, 3.0f}; // Custom shape
			Entity start = addShape(boxId, vars1, mirrorMaterial, CombinationType::Addition, false);
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
