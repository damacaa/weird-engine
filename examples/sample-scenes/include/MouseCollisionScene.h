#pragma once

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

	// Inherited via Scene
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		for (size_t i = 0; i < 9900; i++)
		{

			float y = static_cast<float>(i / 20);
			float x = 5 + (i % 20) + sin(y);

			int material = 4 + (i % 12);

			float z = 0;

			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			if (i < 100)
			{
			}

			Dot& dot = ecs.addComponent<Dot>(entity);
			dot.materialId = 0;

			RigidBody2D& rb = ecs.addComponent<RigidBody2D>(entity);
			CollisionCounter& counter = ecs.addComponent<CollisionCounter>(entity);
		}

		// Floor
		{
			float variables[8]{0.0f, 1.5f, 1.0f};
			addShape(DefaultShapes::SINE, variables, 3);
		}

		// Wall right
		{
			float variables[8]{30 + 5, 0, 5.0f, 30.0f, 0.0f};
			addShape(DefaultShapes::BOX, variables, 3);
		}

		// Wall left
		{
			float variables[8]{-5, 0, 5.0f, 30.0f, 0.0f};
			addShape(DefaultShapes::BOX, variables, 3);
		}

		{
			float variables[8]{-15.0f, 50.0f, 5.0f, 4.5f, 2.0f, 10.0f};
			Entity star = addShape(DefaultShapes::CIRCLE, variables, 7);

			m_cursorShape = star;
		}

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}

		// Move wall to mouse
		{
			CustomShape& cs = ecs.getComponent<CustomShape>(m_cursorShape);
			auto& cameraTransform = ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.parameters[0] = mousePositionInWorld.x;
			cs.parameters[1] = mousePositionInWorld.y;
			ecs.setComponentDirty(cs);
		}
	}

	void onEntityCollision(ECSManager& ecs, WeirdEngine::EntityCollisionEvent& event) override
	{
		if (ecs.hasComponent<CollisionCounter>(event.entityA))
		{
			auto& counter = ecs.getComponent<CollisionCounter>(event.entityA);
			counter.count++;

			constexpr int COLLISIONS_PER_MATERIAL = 50;
			if (counter.count <= 10 * COLLISIONS_PER_MATERIAL && counter.count % COLLISIONS_PER_MATERIAL == 0)
			{
				auto& dot = ecs.getComponent<Dot>(event.entityA);
				dot.materialId++;

				if (counter.count == 10 * COLLISIONS_PER_MATERIAL)
					dot.materialId = 0;
			}
		}

		if (ecs.hasComponent<CollisionCounter>(event.entityB))
		{
			auto& counter = ecs.getComponent<CollisionCounter>(event.entityB);
			counter.count++;

			constexpr int COLLISIONS_PER_MATERIAL = 50;
			if (counter.count <= 10 * COLLISIONS_PER_MATERIAL && counter.count % COLLISIONS_PER_MATERIAL == 0)
			{
				auto& dot = ecs.getComponent<Dot>(event.entityB);
				dot.materialId++;

				if (counter.count == 10 * COLLISIONS_PER_MATERIAL)
					dot.materialId = 0;
			}
		}
	}

	void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		if (ecs.hasComponent<CollisionCounter>(event.entity))
		{
			auto& counter = ecs.getComponent<CollisionCounter>(event.entity);
			counter.count = 0;
			auto& dot = ecs.getComponent<Dot>(event.entity);
			dot.materialId = 0;
		}
	}
};
