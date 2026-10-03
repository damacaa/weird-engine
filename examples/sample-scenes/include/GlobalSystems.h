#pragma once

#include <weird-engine.h>

#include "globals.h"

// Systems shared by every WeirdSamples scene. Include this header and register
// them in the scene constructor:
//
//   addStartSystem(GlobalSystems::cameraInitSystem);
//   addUpdateSystem(GlobalSystems::cameraTrackingSystem);
//   addUpdateSystem(GlobalSystems::sceneControlSystem);
namespace GlobalSystems
{
	// Advance to the next registered scene with Q or gamepad North.
	inline void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		if (services.input().getKeyDown(WeirdEngine::Input::Q) ||
			services.input().getGamepadButtonDown(WeirdEngine::Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	// Keep g_cameraPositon in sync with the camera so the next scene starts where
	// the previous one left off.
	inline void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	// Restore the camera to the global position when a scene starts.
	inline void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace GlobalSystems
