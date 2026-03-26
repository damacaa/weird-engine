#pragma once

#include <random>

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class ShapeCombinatiosScene : public Scene
{
public:
  ShapeCombinatiosScene(const PhysicsSettings& settings)
      : Scene(settings)
  {
  }



private:

  Entity m_circle;
  Entity m_circle2 = 0;
  Entity m_text = 0;
  float m_circleRadious = 0.0f;
  vec2 m_initialMousePositionInWorld;

  std::vector<Entity> m_uiPoints;

  void onStart() override
  {
  	m_debugInput = true;
  	m_debugFly = true;

    // Floor shape
    {
      float vars0[8] = {0.5f, 2.5f, 1.0f};
      addShape(DefaultShapes::SINE, vars0, 2, CombinationType::Addition, true, 0);
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
        float x = distrib(gen) + 15.0f;
        float y = -2.0f + distribY(gen);

        float vars2[8] = { x, y, 3.0f, 5.0f, 1.0f, 0.0f }; // Custom shape
        addShape(DefaultShapes::BOX, vars2, 4 + i, CombinationType::Addition, true, 1);
      }
    }



    // Circle
    {
      float vars[8] = { 15.0f, 7.5f, 5.0f};
      addShape(DefaultShapes::CIRCLE, vars, 7, CombinationType::Addition, true, 2);
    }

    // Subtract star
    {
      float vars[8] = { -2.5f + 15.0f, 12.5f, 5.0f, 0.5f, 13.0f, 5.0f };
      addShape(DefaultShapes::STAR, vars, 0, CombinationType::SmoothSubtraction, true, 2);
    }


    // Cursor circle
    {
      float vars2[8] = { 250.0f, 10.0f, 0.0f, 0.0f, 0.0f, 0.0f }; // Custom shape
      m_circle = addShape(DefaultShapes::CIRCLE, vars2, 0, CombinationType::Subtraction, true, CustomShape::GLOBAL_GROUP);
    }

    {
      float vars2[8] = { 15.0f, 0.0f, 30.0f, 0.0f, 0.0f, 0.0f }; // Custom shape
      addShape(DefaultShapes::CIRCLE, vars2, 0, CombinationType::Intersection, true, CustomShape::GLOBAL_GROUP);
    }

    for (int i = 0; i < 10; ++i){
      auto ee = m_ecs.createEntity();
      auto& t = m_ecs.addComponent<Transform>(ee);
      t.position = vec3(15.0f, 15.0f, 10.0f);

      auto& ui = m_ecs.addComponent<UIDot>(ee);
      ui.materialId = 4 + (i % 12);


      m_uiPoints.push_back(ee);
    }

    {
      Entity text = m_ecs.createEntity();
      auto& t = m_ecs.addComponent<Transform>(text);
      auto& uiText = m_ecs.addComponent<UITextRenderer>(text);
      uiText.text = "";
      uiText.material = 12;
      t.position = vec3(150.0f, 150.0f, 0.0f);

      m_text = text;
    }

    m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
  }

  void onUpdate(float delta) override
  {
    g_cameraPositon = m_ecs.getComponent<Transform>(m_mainCamera).position;

    if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}


    auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
    float x = Input::GetMouseX();
    float y = Input::GetMouseY();

    {
      auto& text = m_ecs.getComponent<UITextRenderer>(m_text);
      bool hideText = Input::GetKey(Input::LeftAlt);
      text.text = hideText ? "" : "Balls:" + std::to_string(static_cast<int>(m_ecs.getComponentArray<Dot>()->getSize()));
      text.dirty = true;

      auto& textTransform = m_ecs.getComponent<Transform>(m_text);
      textTransform.position.x = x;
      textTransform.position.y = y;
    }

    // Transform mouse coordinates to world space
    vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));



    if(Input::GetMouseButtonDown(Input::RightClick))
    {
      m_initialMousePositionInWorld = mousePositionInWorld;
    }

    if(Input::GetMouseButton(Input::RightClick))
    {
      vec2 v = mousePositionInWorld - m_initialMousePositionInWorld;
      m_circleRadious = (std::min)(10.0f, length(v));
    }
    else
    {
      m_circleRadious -= delta * 10.0f * (m_circleRadious + 1.0f);
      m_circleRadious = (std::max)(0.0f, m_circleRadious);
    }

    {
	    CustomShape& cs = m_ecs.getComponent<CustomShape>(m_circle);
	    cs.parameters[0] = m_initialMousePositionInWorld.x;
	    cs.parameters[1] = m_circleRadious <= 0.0f ? -1000.0f : m_initialMousePositionInWorld.y;
	    cs.parameters[2] = m_circleRadious;

	    cs.isDirty = true;
    }

    if (m_circle2)
    {
	    CustomShape& cs = m_ecs.getComponent<CustomShape>(m_circle2);
	    cs.parameters[0] = m_initialMousePositionInWorld.x;
	    cs.parameters[1] = m_initialMousePositionInWorld.y;
	    cs.parameters[2] = (std::max)(0.0f, m_circleRadious - 0.1f);

	    cs.isDirty = true;
    }



    float volume = AudioEngine::getInstance().getAudioData().currentVolume;
    glm::vec2 center = glm::vec2(75.0f, 75.0f); // Screen center X, Y
    float radius = 50.0f - (volume * 50.0f);    // Distance from center
    float speed = 1.0f;       // How fast they rotate
    float spacing = 2.0f * 3.14f / (static_cast<float>(m_uiPoints.size()));     // Gap between each dot along the circle arc

    for (int i = 0; i < m_uiPoints.size(); i++)
    {
    	// Calculate angle: Time moves them, 'i' spreads them out
    	float angle = (getTime() * speed) + (i * spacing);

    	float x = center.x + std::cos(angle) * radius;
    	float y = center.y + std::sin(angle) * radius;

    	auto& t = m_ecs.getComponent<Transform>(m_uiPoints[i]);
      t.position = vec3(x, y, 0.0f);
    }
  }
};
