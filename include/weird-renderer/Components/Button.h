#pragma once

#include "weird-engine/ecs/Component.h"

#include <bitset>

namespace WeirdEngine
{
	enum class ButtonState {
		Off,
		Down,
		Hold,
		Up
	};

	struct ShapeButton : public Component
	{
		ButtonState state = ButtonState::Off;
		std::bitset<8> parameterModifierMask;
		float modifierAmount = 0.0f;
	};


}
