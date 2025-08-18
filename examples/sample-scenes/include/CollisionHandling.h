#pragma once

#include <weird-engine.h>
#include <random>

using namespace WeirdEngine;
class CollisionHandlingScene : public Scene
{
public:
  CollisionHandlingScene()
      : Scene() {
        };

private:
  // Inherited via Scene
  void onStart() override
  {
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
      float variables[8]{0.5f, 1.5f, -1.0f};
      addShape(0, variables, 3);
    }

    // Wall right
    {
      float variables[8]{30 + 5, 0, 5.0f, 30.0f, 0.0f};
      addShape(m_sdfs.size() - 1, variables, 3);
    }

    // Wall left
    {
      float variables[8]{-5, 0, 5.0f, 30.0f, 0.0f};
      addShape(m_sdfs.size() - 1, variables, 3);
    }

    m_ecs.getComponent<FlyMovement2D>(m_mainCamera).targetPosition = vec3(15.0f, 15.0f, 10.0f);
    m_ecs.getComponent<Transform>(m_mainCamera).position = vec3(15.0f, 15.0f, 10.0f);
  }

  void onUpdate(float delta) override
  {
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
    m_simulation2D.addForce(event.bodyB, vec2(0.0f, 10.0f));

    // Entity a = m_ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(event.bodyA).Owner;
    // Transform &at = m_ecs.getComponent<Transform>(a);
    // at.position.x = 15.0f + sinf(event.bodyB * 123.4565f + t);
    // at.position.y = 5.0f + (event.bodyA % 10) * 2.5f;
    // at.isDirty = true;
  }
};
