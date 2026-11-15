#pragma once
#include "weird-audio/AudioEngine.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Input.h"
#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"

#include <memory>
#include <vector>

namespace WeirdEngine
{
	using namespace ECS;

	namespace ButtonSystem
	{
		struct ButtonTransition
		{
			bool enteredFromOff = false;
			bool exitedToOff = false;
		};

		struct TextBounds
		{
			float minX = 0.0f;
			float minY = 0.0f;
			float maxX = 0.0f;
			float maxY = 0.0f;
		};

		inline ButtonTransition stepMomentaryButton(ButtonState& state, bool hovered, bool mouseIsClicking,
													bool respectUIClick)
		{
			ButtonTransition transition{};
			if (mouseIsClicking && hovered)
			{
				const bool blockedByPriorUI =
					respectUIClick && Input::isUIClick() && (state == ButtonState::Off || state == ButtonState::Up);
				if (!blockedByPriorUI)
				{
					switch (state)
					{
						case ButtonState::Off:
							state = ButtonState::Down;
							transition.enteredFromOff = true;
							break;
						case ButtonState::Up:
							state = ButtonState::Down;
							break;
						case ButtonState::Down:
						case ButtonState::Hold:
							state = ButtonState::Hold;
							break;
					}
					Input::flagUIClick();
				}
			}

			if (!mouseIsClicking)
			{
				switch (state)
				{
					case ButtonState::Off:
						break;
					case ButtonState::Down:
					case ButtonState::Hold:
						state = ButtonState::Up;
						break;
					case ButtonState::Up:
						state = ButtonState::Off;
						transition.exitedToOff = true;
						break;
				}
			}

			return transition;
		}

		inline ButtonTransition stepToggleState(ButtonState& state, bool& active, bool hovered, bool mouseDown,
												bool respectUIClick)
		{
			if (mouseDown && hovered && !(respectUIClick && Input::isUIClick()))
			{
				active = !active;
				Input::flagUIClick();
			}

			ButtonTransition transition{};
			if (active)
			{
				switch (state)
				{
					case ButtonState::Off:
						state = ButtonState::Down;
						transition.enteredFromOff = true;
						break;
					case ButtonState::Up:
						state = ButtonState::Down;
						break;
					case ButtonState::Down:
						state = ButtonState::Hold;
						break;
					case ButtonState::Hold:
						break;
				}
			}
			else
			{
				switch (state)
				{
					case ButtonState::Off:
						break;
					case ButtonState::Down:
					case ButtonState::Hold:
						state = ButtonState::Up;
						break;
					case ButtonState::Up:
						state = ButtonState::Off;
						transition.exitedToOff = true;
						break;
				}
			}

			return transition;
		}

		inline bool computeTextBounds(const UITextRenderer& text, const Transform& transform,
									  const SDFRenderSystemContext& ctx, TextBounds& outBounds)
		{
			const int charCount = static_cast<int>(text.text.length());
			if (charCount <= 0)
			{
				return false;
			}

			const float charWidth = static_cast<float>(ctx.font.getCharWidth()) * 2.0f * ctx.dotRadious;
			const float width =
				(static_cast<float>(charCount) * charWidth) + (static_cast<float>(charCount - 1) * ctx.charSpacing);
			const float height = static_cast<float>(ctx.font.getCharHeight()) * 2.0f * ctx.dotRadious;

			float horizontalOffset = 0.0f;
			switch (text.horizontalAlignment)
			{
				case TextRenderer::HorizontalAlignment::Left:
					horizontalOffset = 0.0f;
					break;
				case TextRenderer::HorizontalAlignment::Center:
					horizontalOffset = -width * 0.5f;
					break;
				case TextRenderer::HorizontalAlignment::Right:
					horizontalOffset = -width;
					break;
			}

			float verticalOffset = 0.0f;
			switch (text.verticalAlignment)
			{
				case TextRenderer::VerticalAlignment::Bottom:
					verticalOffset = 0.0f;
					break;
				case TextRenderer::VerticalAlignment::Center:
					verticalOffset = -height * 0.5f;
					break;
				case TextRenderer::VerticalAlignment::Top:
					verticalOffset = -height;
					break;
			}

			outBounds.minX = transform.position.x + horizontalOffset;
			outBounds.minY = transform.position.y + verticalOffset;
			outBounds.maxX = outBounds.minX + width;
			outBounds.maxY = outBounds.minY + height;
			return true;
		}

		inline bool isPointInsideText(const UITextRenderer& text, const Transform& transform,
									  const SDFRenderSystemContext& ctx, float mx, float my, float padding)
		{
			TextBounds bounds{};
			if (!computeTextBounds(text, transform, ctx, bounds))
			{
				return false;
			}
			return mx >= (bounds.minX - padding) && mx <= (bounds.maxX + padding) && my >= (bounds.minY - padding) &&
				   my <= (bounds.maxY + padding);
		}

		inline void applyShapeParameterModifier(UIShape& shape, const std::bitset<8>& mask, float delta)
		{
			for (int j = 0; j < 8; ++j)
			{
				if (mask.test(j))
				{
					shape.parameters[j] += delta;
				}
			}
		}

		inline void updateButtons(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			const bool mouseIsClicking = Input::GetMouseButton(Input::LeftClick);
			const vec2 mousePos(Input::GetMouseX(), Input::GetMouseY());
			const float audioVolume = WeirdAudio::AudioEngine::getInstance().getAudioVolume();

			registry.forEach<ShapeButton, UIShape>(
				[&](Entity buttonOwner, ShapeButton& buttonComponent, UIShape& shape)
				{
					float parameters[12];
					packSdfParameters(parameters, shape.parameters, time, mousePos, audioVolume);

					const float distance = sdfs[shape.distanceFieldId]->getValue(parameters);
					buttonComponent.hovered = distance < buttonComponent.clickPadding;

					const ButtonTransition transition =
						stepMomentaryButton(buttonComponent.state, buttonComponent.hovered, mouseIsClicking, false);
					if (transition.enteredFromOff)
					{
						applyShapeParameterModifier(shape, buttonComponent.parameterModifierMask,
													buttonComponent.modifierAmount);
					}
					else if (transition.exitedToOff)
					{
						applyShapeParameterModifier(shape, buttonComponent.parameterModifierMask,
													-buttonComponent.modifierAmount);
					}
				});
		}

		inline void updateToggles(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			const bool mouseIsClicking = Input::GetMouseButtonDown(Input::LeftClick);
			const vec2 mousePos(Input::GetMouseX(), Input::GetMouseY());
			const float audioVolume = WeirdAudio::AudioEngine::getInstance().getAudioVolume();

			registry.forEach<ShapeToggle, UIShape>(
				[&](Entity toggleOwner, ShapeToggle& toggleComponent, UIShape& shape)
				{
					float parameters[12];
					packSdfParameters(parameters, shape.parameters, time, mousePos, audioVolume);

					const float distance = sdfs[shape.distanceFieldId]->getValue(parameters);
					toggleComponent.hovered = distance < toggleComponent.clickPadding;

					const ButtonTransition transition = stepToggleState(
						toggleComponent.state, toggleComponent.active, toggleComponent.hovered, mouseIsClicking, false);
					if (transition.enteredFromOff)
					{
						applyShapeParameterModifier(shape, toggleComponent.parameterModifierMask,
													toggleComponent.modifierAmount);
					}
					else if (transition.exitedToOff)
					{
						applyShapeParameterModifier(shape, toggleComponent.parameterModifierMask,
													-toggleComponent.modifierAmount);
					}
				});
		}

		inline void updateTextButtons(Registry& registry, const SDFRenderSystemContext& uiContext)
		{
			const bool mouseIsClicking = Input::GetMouseButton(Input::LeftClick);
			const float mx = Input::GetMouseX();
			const float my = Input::GetMouseY();

			registry.forEach<TextButton, UITextRenderer, Transform>(
				[&](Entity buttonOwner, TextButton& buttonComponent, UITextRenderer& text, Transform& transform)
				{
					buttonComponent.hovered =
						isPointInsideText(text, transform, uiContext, mx, my, buttonComponent.clickPadding);

					stepMomentaryButton(buttonComponent.state, buttonComponent.hovered, mouseIsClicking, true);

					const bool hasCustomMaterials = (buttonComponent.hoverMaterial != TextButton::UNSET_MATERIAL) ||
													(buttonComponent.pressedMaterial != TextButton::UNSET_MATERIAL);
					if (!hasCustomMaterials)
						return;

					if (buttonComponent.baseMaterial == TextButton::UNSET_MATERIAL)
					{
						buttonComponent.baseMaterial = text.material;
					}

					uint16_t targetMat = buttonComponent.baseMaterial;
					if ((buttonComponent.state == ButtonState::Down || buttonComponent.state == ButtonState::Hold) &&
						buttonComponent.pressedMaterial != TextButton::UNSET_MATERIAL)
					{
						targetMat = buttonComponent.pressedMaterial;
					}
					else if (buttonComponent.hovered && buttonComponent.hoverMaterial != TextButton::UNSET_MATERIAL)
					{
						targetMat = buttonComponent.hoverMaterial;
					}
					text.material = targetMat;
				});
		}

		inline void updateTextToggles(Registry& registry, const SDFRenderSystemContext& uiContext)
		{
			const bool mouseDown = Input::GetMouseButtonDown(Input::LeftClick);
			const float mx = Input::GetMouseX();
			const float my = Input::GetMouseY();

			registry.forEach<TextToggle, UITextRenderer, Transform>(
				[&](Entity toggleOwner, TextToggle& toggleComponent, UITextRenderer& text, Transform& transform)
				{
					toggleComponent.hovered =
						isPointInsideText(text, transform, uiContext, mx, my, toggleComponent.clickPadding);

					stepToggleState(toggleComponent.state, toggleComponent.active, toggleComponent.hovered, mouseDown,
									true);

					const bool hasCustomMaterials = (toggleComponent.activeMaterial != TextToggle::UNSET_MATERIAL) ||
													(toggleComponent.inactiveMaterial != TextToggle::UNSET_MATERIAL) ||
													(toggleComponent.hoverMaterial != TextToggle::UNSET_MATERIAL);
					if (!hasCustomMaterials)
						return;

					if (toggleComponent.baseMaterial == TextToggle::UNSET_MATERIAL)
					{
						toggleComponent.baseMaterial = text.material;
					}

					uint16_t targetMat = toggleComponent.baseMaterial;
					if (toggleComponent.hovered && toggleComponent.hoverMaterial != TextToggle::UNSET_MATERIAL)
					{
						targetMat = toggleComponent.hoverMaterial;
					}
					else if (toggleComponent.active && toggleComponent.activeMaterial != TextToggle::UNSET_MATERIAL)
					{
						targetMat = toggleComponent.activeMaterial;
					}
					else if (!toggleComponent.active && toggleComponent.inactiveMaterial != TextToggle::UNSET_MATERIAL)
					{
						targetMat = toggleComponent.inactiveMaterial;
					}
					text.material = targetMat;
				});
		}

		inline void update(Registry& registry, std::vector<std::shared_ptr<IMathExpression>>& sdfs,
						   const SDFRenderSystemContext& uiContext, float time)
		{
			updateButtons(registry, sdfs, time);
			updateToggles(registry, sdfs, time);
			updateTextButtons(registry, uiContext);
			updateTextToggles(registry, uiContext);
		}
	} // namespace ButtonSystem
} // namespace WeirdEngine
