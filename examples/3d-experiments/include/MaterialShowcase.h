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
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugFly(true);

		{
			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(-0.5f, -2.0f, 0);

			auto& mat = services.materials3D().createMaterial("specular_ball");
			mat.color = ColorPalette::White;
			mat.metallic = 1.0f;
			mat.roughness = 0.0f;

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = mat.id;
		}

		std::vector<uint16_t> randomMats;

		{
			auto& mat = services.materials3D().createMaterial("orange_smooth");
			mat.color = ColorPalette::Orange;
			mat.metallic = 0.5f;
			mat.roughness = 0.1f;
			mat.pattern = MaterialPattern::None;
			mat.secondaryColor = vec4(0.0f);

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = services.materials3D().createMaterial("green_checkers");
			mat.color = ColorPalette::LightGreen;
			mat.metallic = 0.05f;
			mat.roughness = 0.99f;
			mat.pattern = MaterialPattern::Checkers;
			mat.secondaryColor = mat.color * 0.8f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = services.materials3D().createMaterial("magenta_waves");
			mat.color = ColorPalette::Magenta;
			mat.secondaryColor = ColorPalette::Orange;
			mat.metallic = 0.5f;
			mat.roughness = 0.05f;
			mat.pattern = MaterialPattern::Waves;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = services.materials3D().createMaterial("cyan_checkers");
			mat.color = ColorPalette::Cyan;
			mat.metallic = 0.05f;
			mat.roughness = 0.99f;
			mat.pattern = MaterialPattern::Checkers;
			mat.secondaryColor = mat.color * 0.8f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = services.materials3D().createMaterial("blue_noise");
			mat.color = ColorPalette::LightBlue;
			mat.secondaryColor = ColorPalette::White;
			mat.metallic = 0.3f;
			mat.roughness = 0.001f;
			mat.pattern = MaterialPattern::PerlinNoise;
			mat.patternScale = 5.0f;

			randomMats.push_back(mat.id);
		}

		{
			auto& mat = services.materials3D().createMaterial("gold");
			mat.color = vec4(ColorPalette::Yellow, 0.5f);
			mat.metallic = 0.5f;
			mat.roughness = 0.0f;

			randomMats.push_back(mat.id);
		}

		for (size_t i = 0; i < randomMats.size(); i++)
		{
			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(1.0f + (i), -2.0f, 0);

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = randomMats[i];
		}

		auto& floorMaterial = services.materials3D().createMaterial("floor");
		floorMaterial.color = ColorPalette::White;
		floorMaterial.metallic = 0.1f;
		floorMaterial.roughness = 0.3f;
		floorMaterial.pattern = MaterialPattern::Checkers;
		floorMaterial.secondaryColor = floorMaterial.color * 0.8f;

		services.shapes().addShape({.shapeId = DefaultShapes3D::PLANE,
									.variables = {3},
									.material = floorMaterial,
									.combination = CombinationType::Addition,
									.hasCollision = false});

		auto& mirrorMaterial = services.materials3D().createMaterial();
		mirrorMaterial.color = vec4(1.0f);
		mirrorMaterial.metallic = 1.0f;
		mirrorMaterial.roughness = 0.0f;

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, -5.0f},
													  {Primitives3D::Box::POS_Y, -2.0f},
													  {Primitives3D::Box::POS_Z, 0.0f},
													  {Primitives3D::Box::SIZE_X, 0.1f},
													  {Primitives3D::Box::SIZE_Y, 1.0f},
													  {Primitives3D::Box::SIZE_Z, 3.0f}},
										.material = mirrorMaterial,
										.combination = CombinationType::Addition,
										.hasCollision = false});
		}

		{
			std::shared_ptr<IMathExpression> box = std::make_shared<Primitives3D::Box>();
			auto boxId = services.shapes().registerSDF(box);

			services.shapes().addShape({.shapeId = boxId,
										.variables = {{Primitives3D::Box::POS_X, 20.0f},
													  {Primitives3D::Box::POS_Y, -2.0f},
													  {Primitives3D::Box::POS_Z, 0.0f},
													  {Primitives3D::Box::SIZE_X, 0.1f},
													  {Primitives3D::Box::SIZE_Y, 1.0f},
													  {Primitives3D::Box::SIZE_Z, 3.0f}},
										.material = mirrorMaterial,
										.combination = CombinationType::Addition,
										.hasCollision = false});
		}

		{
			m_sunLight = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(m_sunLight);
			t.position = glm::vec3(0.0f, 0.0f, 0.0f);
			t.rotation = normalize(glm::vec3(0.0f, 0.4f, 1.0f));

			Light3DComponent& lc = registry.addComponent<Light3DComponent>(m_sunLight);
			lc.type = Light3DType::Directional;
			lc.color = glm::vec4(1.0f, 1.0f, 1.0f, 0.75f);
		}

		// getLights().push_back(
		// 	Light{1, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.35f, 0.45f, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)});

		// getLights().push_back(
		// 	Light{2, glm::vec3(0.0f, 0.0f, 0.0f), 0, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec4(0.0f, 0.0f, 2.0f, 10.0f)});

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(12, -1, 12);
		cameraTransform.rotation.x = -0.95f;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q))
		{
			services.sceneControl().goToNextScene();
		}
	}
};
