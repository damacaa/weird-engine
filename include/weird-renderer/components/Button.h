#pragma once

#include <bitset>
#include <cstdint>

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
		// When true, the click must start while hovering
		bool requireClickStart = true;
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

	struct TextButton
	{
		static constexpr uint16_t UNSET_MATERIAL = 0xFFFF;

		ButtonState state = ButtonState::Off;
		float clickPadding = 10.0f;
		bool hovered = false;
		// When true, the click must start while hovering
		bool requireClickStart = true;
		uint16_t baseMaterial = UNSET_MATERIAL;
		uint16_t hoverMaterial = UNSET_MATERIAL;
		uint16_t pressedMaterial = UNSET_MATERIAL;
	};

	struct TextToggle
	{
		static constexpr uint16_t UNSET_MATERIAL = 0xFFFF;

		ButtonState state = ButtonState::Off;
		bool active = false;
		float clickPadding = 10.0f;
		bool hovered = false;
		uint16_t baseMaterial = UNSET_MATERIAL;
		uint16_t activeMaterial = UNSET_MATERIAL;
		uint16_t inactiveMaterial = UNSET_MATERIAL;
		uint16_t hoverMaterial = UNSET_MATERIAL;
	};

} // namespace WeirdEngine
