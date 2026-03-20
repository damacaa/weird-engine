#pragma once

#include <weird-engine.h>
#include <random>

#include "globals.h"

using namespace WeirdEngine;
class CollisionHandlingScene : public Scene
{
public:
  CollisionHandlingScene(const PhysicsSettings& settings)
      : Scene(settings) {
        };

private:
  // Inherited via Scene
  void onStart() override
  {
  	m_debugInput = true;
  	m_debugFly = true;

    // Create a random number generator engine

    for (size_t i = 0; i < 10; i++)
    {
      float y = 10 + i;
      float x = 2 * i;

      int material = 4 + (i % 12);

      float z = 0;

      Entity entity = m_ecs.createEntity();
      Transform &t = m_ecs.addComponent<Transform>(entity);
      t.position = vec3(x + 0.5f, y + 0.5f, z);

      SDFRenderer &sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
      sdfRenderer.materialId = material;

      RigidBody2D &rb = m_ecs.addComponent<RigidBody2D>(entity);
    }

    // Floor
    {
      float variables[8]{15.0f, 5.0f, 25.0f};
		addShape(DefaultShapes::CIRCLE, variables, 3);
	}

	{
		float variables[8]{15.0f, -50.0f, 250.0f, 50.0f};
		auto floor = addShape(DefaultShapes::BOX, variables, 3, CombinationType::SmoothAddition);
		m_ecs.getComponent<CustomShape>(floor).smoothFactor = 3.0f;
	}

	{
		float variables[8]{15.0f, 5.0f, 20.0f};
		addShape(DefaultShapes::CIRCLE, variables, 3, CombinationType::Subtraction);
	}

    m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
  }

  void onUpdate(float delta) override
  {
    if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}
  }

  float m_lastTime = 0.0f;
  void onCollision(WeirdEngine::CollisionEvent &event) override
  {
    float t = getTime();
    if (t - m_lastTime < 0.1f)
      return; // Avoid multiple collisions in a short time

    m_lastTime = t;
    // m_simulation2D.setPosition(event.bodyA, vec2(15.0f, 15.0f));
    // m_simulation2D.addForce(event.bodyA, vec2(2.0f * sinf(t), -20.0f));
	m_simulation2D.setPosition(event.bodyB, vec2(15.0f, 24.45f));
	m_simulation2D.addForce(event.bodyB, vec2(20.0f * sinf(10.0f * t), -30.0f));

	// Entity a = m_ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(event.bodyA).Owner;
    // Transform &at = m_ecs.getComponent<Transform>(a);
    // at.position.x = 15.0f + sinf(event.bodyB * 123.4565f + t);
    // at.position.y = 5.0f + (event.bodyA % 10) * 2.5f;
    // at.isDirty = true;
  }
};
