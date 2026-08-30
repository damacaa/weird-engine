#pragma once

#include "weird-renderer/audio/AudioPresets.h"
#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;

class MouseCollisionScene : public Scene2D
{
public:
	MouseCollisionScene() {};

private:
	struct CollisionCounter
	{
		int count;
	};

	Entity m_cursorShape;
	Material2DHandle m_baseDotMat;
	std::vector<Material2DHandle> m_hitMats;

	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio module with mouse_collision preset
		auto& audioModule = services.audio().getAudioModule();
		if (audioModule)
		{
			WeirdEngine::WeirdRenderer::setAudioModuleFromPreset(audioModule, "mouse_collision");
		}

		auto& baseMat = services.materials2D().createMaterial("dot_base");
		baseMat.color = ColorPalette::Black;
		m_baseDotMat = baseMat.id;

		auto& wallMat = services.materials2D().createMaterial("wall");
		wallMat.color = ColorPalette::LightGray;

		auto& cursorMat = services.materials2D().createMaterial("cursor");
		cursorMat.color = vec4(ColorPalette::Yellow, 0.5f);

		for (int i = 0; i < 10; ++i)
		{
			auto& mat = services.materials2D().createMaterial("hit_" + std::to_string(i));
			mat.color = ColorPalette::Default[(6 + i) % ColorPalette::Default.size()];
			m_hitMats.push_back(mat.id);
		}

		for (size_t i = 0; i < 9900; i++)
		{

			float y = static_cast<float>(i / 20);
			float x = 5 + (i % 20) + sin(y);

			float z = 0;

			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Dot& dot = registry.addComponent<Dot>(entity);
			dot.materialId = m_baseDotMat.id;

			RigidBody2D& rb = registry.addComponent<RigidBody2D>(entity);
			CollisionCounter& counter = registry.addComponent<CollisionCounter>(entity);
		}

		// Floor
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.0f},
												  {Primitives::SineWave::PERIOD, 1.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = wallMat});

		// Wall right
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 35.0f},
												  {Primitives::Box::POS_Y, 0.0f},
												  {Primitives::Box::SIZE_X, 5.0f},
												  {Primitives::Box::SIZE_Y, 30.0f}},
									.material = wallMat});

		// Wall left
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, -5.0f},
												  {Primitives::Box::POS_Y, 0.0f},
												  {Primitives::Box::SIZE_X, 5.0f},
												  {Primitives::Box::SIZE_Y, 30.0f}},
									.material = wallMat});

		m_cursorShape = services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
													.variables = {{Primitives::Circle::POS_X, -15.0f},
																  {Primitives::Circle::POS_Y, 50.0f},
																  {Primitives::Circle::RADIUS, 5.0f}},
													.material = cursorMat});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		// Move wall to mouse
		{
			CustomShape& cs = registry.getComponent<CustomShape>(m_cursorShape);
			auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
			float x = services.input().getMouseX();
			float y = services.input().getMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.parameters[0] = mousePositionInWorld.x;
			cs.parameters[1] = mousePositionInWorld.y;
			registry.setComponentDirty(cs);
		}
	}

	void onEntityCollision(Registry& registry, ServiceProvider& services,
						   WeirdEngine::EntityCollisionEvent& event) override
	{
		if (registry.hasComponent<CollisionCounter>(event.entityA))
		{
			auto& counter = registry.getComponent<CollisionCounter>(event.entityA);
			counter.count++;

			constexpr int COLLISIONS_PER_MATERIAL = 50;
			if (counter.count <= 10 * COLLISIONS_PER_MATERIAL && counter.count % COLLISIONS_PER_MATERIAL == 0)
			{
				auto& dot = registry.getComponent<Dot>(event.entityA);
				int stage = (counter.count / COLLISIONS_PER_MATERIAL) - 1;
				if (stage >= 0 && stage < static_cast<int>(m_hitMats.size()))
				{
					dot.materialId = m_hitMats[stage].id;
				}

				if (counter.count == 10 * COLLISIONS_PER_MATERIAL)
					dot.materialId = m_baseDotMat.id;
			}
		}

		if (registry.hasComponent<CollisionCounter>(event.entityB))
		{
			auto& counter = registry.getComponent<CollisionCounter>(event.entityB);
			counter.count++;

			constexpr int COLLISIONS_PER_MATERIAL = 50;
			if (counter.count <= 10 * COLLISIONS_PER_MATERIAL && counter.count % COLLISIONS_PER_MATERIAL == 0)
			{
				auto& dot = registry.getComponent<Dot>(event.entityB);
				int stage = (counter.count / COLLISIONS_PER_MATERIAL) - 1;
				if (stage >= 0 && stage < static_cast<int>(m_hitMats.size()))
				{
					dot.materialId = m_hitMats[stage].id;
				}

				if (counter.count == 10 * COLLISIONS_PER_MATERIAL)
					dot.materialId = m_baseDotMat.id;
			}
		}
	}

	void onEntityShapeCollision(Registry& registry, ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		if (registry.hasComponent<CollisionCounter>(event.entity))
		{
			auto& counter = registry.getComponent<CollisionCounter>(event.entity);
			counter.count = 0;
			auto& dot = registry.getComponent<Dot>(event.entity);
			dot.materialId = m_baseDotMat.id;
		}
	}
};
