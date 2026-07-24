#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
class CollisionHandlingScene : public Scene2D
{
public:
	CollisionHandlingScene() {};

private:
	// Inherited via Scene
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		// Create a random number generator engine

		for (size_t i = 0; i < 10; i++)
		{
			float y = 10.0f + static_cast<float>(i);
			float x = 2.0f * static_cast<float>(i);

			int material = 4 + (i % 12);

			float z = 0;

			Entity entity = ecs.createEntity();
			Transform& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Dot& dot = ecs.addComponent<Dot>(entity);
			dot.materialId = material;

			RigidBody2D& rb = ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{15.0f, 5.0f, 25.0f};
			addShape(DefaultShapes::CIRCLE, variables, 3);
		}

		{
			float variables[8]{15.0f, -50.0f, 250.0f, 50.0f};
			auto floor = addShape(DefaultShapes::BOX, variables, 3, CombinationType::SmoothAddition);
			ecs.getComponent<CustomShape>(floor).smoothFactor = 3.0f;
		}

		{
			float variables[8]{15.0f, 5.0f, 20.0f};
			addShape(DefaultShapes::CIRCLE, variables, 3, CombinationType::Subtraction);
		}

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}
	}

	float m_lastTime = 0.0f;
	void onCollision(Simulation2D& simulation, WeirdEngine::CollisionEvent& event) override
	{
		float t = getTime();
		if (t - m_lastTime < 0.1f)
			return; // Avoid multiple collisions in a short time

		m_lastTime = t;
		// simulation.setPosition(event.bodyA, vec2(15.0f, 15.0f));
		// simulation.addImpulseForce(event.bodyA, vec2(2.0f * sinf(t), -20.0f));
		simulation.setPosition(event.bodyB, vec2(15.0f, 24.45f));
		simulation.addImpulseForce(event.bodyB, vec2(20.0f * sinf(10.0f * t), -30.0f));

		// Entity a = ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(event.bodyA).Owner;
		// Transform &at = ecs.getComponent<Transform>(a);
		// at.position.x = 15.0f + sinf(event.bodyB * 123.4565f + t);
		// at.position.y = 5.0f + (event.bodyA % 10) * 2.5f;
		// at.isDirty = true;
	}
};
