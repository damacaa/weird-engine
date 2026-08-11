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
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Create a random number generator engine

		for (size_t i = 0; i < 10; i++)
		{
			float y = 10.0f + static_cast<float>(i);
			float x = 2.0f * static_cast<float>(i);

			int material = 4 + (i % 12);

			float z = 0;

			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Dot& dot = registry.addComponent<Dot>(entity);
			dot.materialId = material;

			RigidBody2D& rb = registry.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{15.0f, 5.0f, 25.0f};
			services.shapes().addShape(DefaultShapes::CIRCLE, variables, 3);
		}

		{
			float variables[8]{15.0f, -50.0f, 250.0f, 50.0f};
			auto floor = services.shapes().addShape(DefaultShapes::BOX, variables, 3, CombinationType::SmoothAddition);
			registry.getComponent<CustomShape>(floor).smoothFactor = 3.0f;
		}

		{
			float variables[8]{15.0f, 5.0f, 20.0f};
			services.shapes().addShape(DefaultShapes::CIRCLE, variables, 3, CombinationType::Subtraction);
		}

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	float m_currentTime = 0.0f;
	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		m_currentTime = services.time().time();
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	float m_lastTime = 0.0f;
	void onPhysicsRigidBodyCollision(Simulation2D& simulation, WeirdEngine::PhysicsCollisionEvent& event) override
	{
		float t = m_currentTime;
		if (t - m_lastTime < 0.1f)
			return; // Avoid multiple collisions in a short time

		m_lastTime = t;
		// simulation.setPosition(event.bodyA, vec2(15.0f, 15.0f));
		// simulation.addImpulseForce(event.bodyA, vec2(2.0f * sinf(t), -20.0f));
		simulation.setPosition(event.bodyB, vec2(15.0f, 24.45f));
		simulation.addImpulseForce(event.bodyB, vec2(20.0f * sinf(10.0f * t), -30.0f));

		// Entity a = registry.getComponentArray<RigidBody2D>()->getDataAtIdx(event.bodyA).Owner;
		// Transform &at = registry.getComponent<Transform>(a);
		// at.position.x = 15.0f + sinf(event.bodyB * 123.4565f + t);
		// at.position.y = 5.0f + (event.bodyA % 10) * 2.5f;
		// at.isDirty = true;
	}
};
