#pragma once

#include <random>

#include <weird-engine.h>

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class ShapeCombinatiosScene : public Scene
{
public:
  ShapeCombinatiosScene()
      : Scene()
  {
  }



private:

  Entity m_circle;
  Entity m_circle2 = 0;
  float m_circleRadioud = 0.0f;
  vec2 m_initialMousePositionInWorld;

  void onStart() override
  {
    m_debugInput = true;

    // Floor shape
    {
      float vars0[8] = {0.5f, 2.5f};
      addShape(CustomShape::SINE, vars0, 2, CombinationType::Addition, true, 0);
    }

    std::random_device rd;
		std::mt19937 gen(rd());
		float range = 20.0f;
		std::uniform_real_distribution<> distrib(-range, range);

    // Boxes
    {
      std::uniform_real_distribution<> distribY(0, 5);

      for (int i = 0; i < 0; ++i)
      {
        float x = distrib(gen);
        float y = -2.0f + distribY(gen);

        float vars2[8] = { x, y, 3.0f, 5.0f, 1.0f, 0.0f }; // Custom shape
        addShape(CustomShape::BOX, vars2, 4 + i, CombinationType::Addition, true, 1);
      }
    }



    // Circle
    {
      float vars[8] = { 0.0f, 7.5f, 5.0f};
      addShape(CustomShape::CIRCLE, vars, 3, CombinationType::Addition, true, 2);
    }

    // Subtract star
    {
      float vars[8] = { -2.5f, 12.5f, 5.0f, 0.5f, 13.0f, 5.0f };
      addShape(CustomShape::STAR, vars, 0, CombinationType::Subtraction, true, 2);
    }


    // Cursor circle
    {
      float vars2[8] = { 25.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // Custom shape
      m_circle = addShape(CustomShape::CIRCLE, vars2, 0, CombinationType::Subtraction, true, CustomShape::GLOBAL_GROUP);
    }

    {
      float vars2[8] = { 0.0f, 0.0f, 30.0f, 0.0f, 0.0f, 0.0f }; // Custom shape
      addShape(CustomShape::CIRCLE, vars2, 0, CombinationType::Intersection, true, CustomShape::GLOBAL_GROUP);
    }

  vec3 camPos = vec3(0.0f, 7.5f, 15.0f);
    // m_ecs.getComponent<Transform>(m_mainCamera).position = camPos;
    m_ecs.getComponent<FlyMovement2D>(m_mainCamera).targetPosition = camPos;
  }

  void onUpdate(float delta) override
  {
    auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
    float x = Input::GetMouseX();
    float y = Input::GetMouseY();

    // Transform mouse coordinates to world space
    vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));



    if(Input::GetMouseButtonDown(Input::RightClick))
    {
      m_initialMousePositionInWorld = mousePositionInWorld;
    }

    if(Input::GetMouseButton(Input::RightClick))
    {
      vec2 v = mousePositionInWorld - m_initialMousePositionInWorld;
      m_circleRadioud = std::min(10.0f, length(v));
    }
    else
    {
      m_circleRadioud -= delta * 10.0f * (m_circleRadioud + 1.0f);
      m_circleRadioud = std::max(0.0f, m_circleRadioud);
    }


    {

      CustomShape& cs = m_ecs.getComponent<CustomShape>(m_circle);
    	cs.m_parameters[0] = m_initialMousePositionInWorld.x;
			cs.m_parameters[1] = m_initialMousePositionInWorld.y;
      cs.m_parameters[2] = m_circleRadioud;

			cs.m_isDirty = true;
    }


    if (m_circle2)
    {
      CustomShape& cs = m_ecs.getComponent<CustomShape>(m_circle2);
    	cs.m_parameters[0] = m_initialMousePositionInWorld.x;
			cs.m_parameters[1] = m_initialMousePositionInWorld.y;
      cs.m_parameters[2] = std::max(0.0f, m_circleRadioud - 0.1f);

			cs.m_isDirty = true;
    }
  }

  // void onCollision(WeirdEngine::CollisionEvent &event) override
  // {
  //   m_simulation2D.fix(event.bodyB);
  // }
};
