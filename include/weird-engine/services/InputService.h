#pragma once

#include "weird-engine/Input.h"

namespace WeirdEngine
{
	class InputService
	{
	public:
		bool getKey(Input::KeyCode key) const
		{
			return Input::GetKey(key);
		}
		bool getKeyDown(Input::KeyCode key) const
		{
			return Input::GetKeyDown(key);
		}
		bool getKeyUp(Input::KeyCode key) const
		{
			return Input::GetKeyUp(key);
		}

		float getMouseX() const
		{
			return Input::GetMouseX();
		}
		float getMouseY() const
		{
			return Input::GetMouseY();
		}
		float getMouseDeltaX() const
		{
			return Input::GetMouseDeltaX();
		}
		float getMouseDeltaY() const
		{
			return Input::GetMouseDeltaY();
		}
		float getMouseDeltaXRaw() const
		{
			return Input::GetMouseDeltaXRaw();
		}
		float getMouseDeltaYRaw() const
		{
			return Input::GetMouseDeltaYRaw();
		}
		bool getMouseButton(Input::MouseButton button) const
		{
			return Input::GetMouseButton(button);
		}
		bool getMouseButtonDown(Input::MouseButton button) const
		{
			return Input::GetMouseButtonDown(button);
		}
		bool getMouseButtonUp(Input::MouseButton button) const
		{
			return Input::GetMouseButtonUp(button);
		}
		void setMousePosition(float x, float y)
		{
			Input::SetMousePosition(x, y);
		}
		void showMouse()
		{
			Input::ShowMouse();
		}
		void hideMouse()
		{
			Input::HideMouse();
		}
		bool isUIClick() const
		{
			return Input::isUIClick();
		}
		void flagUIClick()
		{
			Input::flagUIClick();
		}

		bool getGamepadButton(Input::GamepadButton button) const
		{
			return Input::GetGamepadButton(button);
		}
		bool getGamepadButtonDown(Input::GamepadButton button) const
		{
			return Input::GetGamepadButtonDown(button);
		}
		bool getGamepadButtonUp(Input::GamepadButton button) const
		{
			return Input::GetGamepadButtonUp(button);
		}
		float getGamepadAxis(Input::GamepadAxis axis) const
		{
			return Input::GetGamepadAxis(axis);
		}

		void suppressMouseInput()
		{
			Input::suppressMouseInput();
		}
		void suppressKeyboardInput()
		{
			Input::suppressKeyboardInput();
		}
	};
} // namespace WeirdEngine
