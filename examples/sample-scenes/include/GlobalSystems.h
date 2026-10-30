#pragma once

#include <weird-engine.h>

// Systems shared by every WeirdSamples scene. Include this header and register
// them in the scene constructor:
//
//   addUpdateSystem(GlobalSystems::sceneControlSystem);
namespace GlobalSystems
{
	// Return to menu scene with ESC or controller Start button.
	inline void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		if (services.input().getKeyDown(WeirdEngine::Input::Esc) ||
			services.input().getGamepadButtonDown(WeirdEngine::Input::GamepadButton::Start))
		{
			services.sceneControl().goToNextScene("menu");
		}
	}
} // namespace GlobalSystems
