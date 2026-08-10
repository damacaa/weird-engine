#pragma once

#include <weird-engine.h>
#include <weird-engine/math/Default2DSDFs.h>

#include <weird-engine/math/Default3DSDFs.h>

using namespace WeirdEngine;

// Cornell Box
class MaterialShowcaseScene : public Scene3D
{
public:
	MaterialShowcaseScene() {};

private:
	Entity m_sunLight;
	// Inherited via Scene
	void onStart(ECSManager& ecs, ServiceProvider& services) override
	{
		services.debug().setDebugFly(true);

		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(-0.5f, -2.0f, 0);

			auto& mat = createMaterial();
			mat.color = vec4(1.0f);
			mat.metallic = 1.0f;
			mat.roughness = 0.0f;

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = mat.id;
		}

		std::vector<uint16_t> randomMats;
		vec4 colors[] = {
			vec4(.95f, 0.4f, 0.1f, 1.0f), // Orange
			vec4(0.5f, 0.0f, 1.0f, 1.0f), // Purple
			vec4(0.0f, .9f, .9f, 1.0f),	  // Cyan
			vec4(0.5f, 1.0f, 0.5f, 1.0f), // Light Green
			vec4(1.0f, 0.3f, .6f, 1.0f),  // Magenta
			vec4(1.0f, 0.5f, 0.5f, 1.0f), // Pink
			vec4(0.5f, 0.5f, 1.0f, 1.0f), // Light Blue
			vec4(0.4f, 0.25f, 0.1f, 1.0f) // Brown
		};

		{
			auto& mat = createMaterial();
			mat.color = vec4(.95f, 0.4f, 0.1f, 1.0f);
			mat.metallic = 0.5f;
			mat.roughness = 0.1f;
			mat.pattern = MaterialPattern::None;
			mat.secondaryColor = vec4(0.0f);

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = createMaterial();
			mat.color = vec4(0.5f, 1.0f, 0.5f, 1.0f);
			mat.metallic = 0.05f;
			mat.roughness = 0.99f;
			mat.pattern = MaterialPattern::Checkers;
			mat.secondaryColor = mat.color * 0.8f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = createMaterial();
			mat.color = vec4(1.0f, 0.3f, .6f, 1.0f);
			mat.secondaryColor = vec4(1.0f, 0.2f, 0.05f, 1.0f);

			mat.metallic = 0.5f;
			mat.roughness = 0.05f;
			mat.pattern = MaterialPattern::Waves;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = createMaterial();
			mat.color = vec4(0.0f, 10.9f, 10.9f, 1.0f);
			mat.metallic = 0.05f;
			mat.roughness = 0.99f;
			mat.pattern = MaterialPattern::Checkers;
			mat.secondaryColor = mat.color * 0.8f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = createMaterial();
			mat.color = vec4(0.5f, 0.5f, 0.8f, 1.0f);
			mat.secondaryColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			mat.metallic = 0.3f;
			mat.roughness = 0.001f;
			mat.pattern = MaterialPattern::PerlinNoise;
			mat.patternScale = 5.0f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = createMaterial();
			mat.color = vec4(0.85f, 0.7f, 0.1f, 0.5f);
			mat.metallic = 0.5f;
			mat.roughness = 0.0f;

			randomMats.push_back(mat.id);
		}

		for (size_t i = 0; i < randomMats.size(); i++)
		{
			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(1.0f + (i), -2.0f, 0);

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = randomMats[i];
		}

		{
			auto& floorMaterial = createMaterial();
			floorMaterial.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
			floorMaterial.metallic = 0.1f;
			floorMaterial.roughness = 0.3f;
			floorMaterial.pattern = MaterialPattern::Checkers;
			floorMaterial.secondaryColor = floorMaterial.color * 0.8f;

			float vars[8] = {3};
			Entity floor = services.shapes().addShape(DefaultShapes3D::PLANE, vars, floorMaterial,
													  CombinationType::Addition, false);
		}

		auto& mirrorMaterial = createMaterial();
		mirrorMaterial.color = vec4(1.0f);
		mirrorMaterial.metallic = 1.0f;
		mirrorMaterial.roughness = 0.0f;

		{

			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			float vars1[8] = {-5.0f, -2.0f, 0.0f, 0.1f, 1.0f, 3.0f}; // Custom shape
			Entity start = services.shapes().addShape(boxId, vars1, mirrorMaterial, CombinationType::Addition, false);
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			float vars1[8] = {20.0f, -2.0f, 0.0f, 0.1f, 1.0f, 3.0f}; // Custom shape
			Entity start = services.shapes().addShape(boxId, vars1, mirrorMaterial, CombinationType::Addition, false);
		}

		{
			m_sunLight = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(m_sunLight);
			t.position = glm::vec3(0.0f, 0.0f, 0.0f);
			t.rotation = normalize(glm::vec3(0.0f, 0.4f, 1.0f));

			LightComponent& lc = ecs.addComponent<LightComponent>(m_sunLight);
			lc.type = LightType::Directional;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.75f);
		}

		// getLights().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLights().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		auto& cameraTransform = ecs.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(12, -1, 12);
		cameraTransform.rotation.x = -0.95f;
	}

	void onUpdate(ECSManager& ecs, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q))
		{
			services.sceneControl().goToNextScene();
		}
	}
};
