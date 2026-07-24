#pragma once

#include <bitset>

namespace WeirdEngine
{
	enum class ButtonState
	{
		Off,
		Down,
		Hold,
		Up
	};

	struct ShapeButton
	{
		ButtonState state = ButtonState::Off;
		std::bitset<8> parameterModifierMask;
		float modifierAmount = 0.0f;
		float clickPadding = 10.0f;
		bool hovered = false;
	};

	struct ShapeToggle
	{
		ButtonState state = ButtonState::Off;
		std::bitset<8> parameterModifierMask;
		float modifierAmount = 0.0f;
		bool active = false;
		float clickPadding = 10.0f;
		bool hovered = false;
	};

} // namespace WeirdEngine
