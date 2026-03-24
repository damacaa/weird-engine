#pragma once

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
class SceneLoadExample : public Scene
{
public:
  SceneLoadExample(const PhysicsSettings& settings)
      : Scene(settings) {
        };

private:
  // Inherited via Scene
  void onStart() override
  {
  	m_debugInput = true;
  	m_debugFly = true;

    m_ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
  }

  void onUpdate(float delta) override
  {
      if (Input::GetKeyDown(Input::Q)) {
          setSceneComplete();
      }

      if (Input::GetKey(Input::LeftCtrl) && Input::GetKeyDown(Input::S)) {
          saveScene("example.weird");
      }
  }

};
