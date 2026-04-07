#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"
#include "weird-engine/math/MathExpressions.h"

#include <memory>
#include <vector>

namespace WeirdEngine
{
	using namespace ECS;

	namespace ButtonSystem
	{
		inline void updateButtons(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time);
		inline void updateToggles(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time);

		inline void update(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			updateButtons(ecs, sdfs, time);
			updateToggles(ecs, sdfs, time);
		}

		inline void updateButtons(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			auto componentArray = ecs.getComponentArray<ShapeButton>();
			unsigned int size = componentArray->getSize();
			bool mouseIsClicking = Input::GetMouseButton(Input::LeftClick);

			for (size_t i = 0; i < size; i++)
			{
				auto& buttonComponent = componentArray->getDataAtIdx(i);

				{
					float parameters[11];

					auto& shape = ecs.getComponent<UIShape>(buttonComponent.Owner);
					std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

					parameters[8] = time;
					parameters[9] = Input::GetMouseX();
					parameters[10] = Input::GetMouseY();

					sdfs[shape.distanceFieldId]->propagateValues(parameters);

					float distance = sdfs[shape.distanceFieldId]->getValue();
					buttonComponent.hovered = distance < buttonComponent.clickPadding;

					if (mouseIsClicking && buttonComponent.hovered)
					{
						switch (buttonComponent.state)
						{
							case ButtonState::Off:
							case ButtonState::Up:
								buttonComponent.state = ButtonState::Down;

								for (int j = 0; j < 8; ++j)
								{
									if (buttonComponent.parameterModifierMask.test(j))
									{
										shape.parameters[j] += buttonComponent.modifierAmount;
									}
								}
								break;
							case ButtonState::Down:
							case ButtonState::Hold:
								buttonComponent.state = ButtonState::Hold;
								break;
						}

						Input::flagUIClick();
					}
				}

				if (!mouseIsClicking)
				{
					switch (buttonComponent.state)
					{
						case ButtonState::Off:
							break;
						case ButtonState::Down:
						case ButtonState::Hold:
							buttonComponent.state = ButtonState::Up;
							break;
						case ButtonState::Up:
						{
							buttonComponent.state = ButtonState::Off;

							auto& shape = ecs.getComponent<UIShape>(buttonComponent.Owner);
							for (int j = 0; j < 8; ++j)
							{
								if (buttonComponent.parameterModifierMask.test(j))
								{
									shape.parameters[j] -= buttonComponent.modifierAmount;
								}
							}

							break;
						}
					}
				}
			}
		}

		inline void updateToggles(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			auto componentArray = ecs.getComponentArray<ShapeToggle>();
			unsigned int size = componentArray->getSize();
			bool mouseIsClicking = Input::GetMouseButtonDown(Input::LeftClick);

			for (size_t i = 0; i < size; i++)
			{
				auto& toggleComponent = componentArray->getDataAtIdx(i);

				{
					float parameters[11];

					auto& shape = ecs.getComponent<UIShape>(toggleComponent.Owner);
					std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

					parameters[8] = time;
					parameters[9] = Input::GetMouseX();
					parameters[10] = Input::GetMouseY();

					sdfs[shape.distanceFieldId]->propagateValues(parameters);

					float distance = sdfs[shape.distanceFieldId]->getValue();
					toggleComponent.hovered = distance < toggleComponent.clickPadding;

					if (mouseIsClicking && toggleComponent.hovered)
					{
						toggleComponent.active = !toggleComponent.active;
						Input::flagUIClick();
					}
				}

				if (toggleComponent.active)
				{
					switch (toggleComponent.state)
					{
						case ButtonState::Off:
						case ButtonState::Up:
						{
							toggleComponent.state = ButtonState::Down;

							auto& shape = ecs.getComponent<UIShape>(toggleComponent.Owner);
							for (int j = 0; j < 8; ++j)
							{
								if (toggleComponent.parameterModifierMask.test(j))
								{
									shape.parameters[j] += toggleComponent.modifierAmount;
								}
							}
							break;
						}
						case ButtonState::Down:
							toggleComponent.state = ButtonState::Hold;
							break;
						case ButtonState::Hold:
							break;
					}
				}
				else
				{
					switch (toggleComponent.state)
					{
						case ButtonState::Off:
							break;
						case ButtonState::Down:
						case ButtonState::Hold:
							toggleComponent.state = ButtonState::Up;
							break;
						case ButtonState::Up:
						{
							toggleComponent.state = ButtonState::Off;

							auto& shape = ecs.getComponent<UIShape>(toggleComponent.Owner);
							for (int j = 0; j < 8; ++j)
							{
								if (toggleComponent.parameterModifierMask.test(j))
								{
									shape.parameters[j] -= toggleComponent.modifierAmount;
								}
							}

							break;
						}
					}
				}
			}
		}
	} // namespace ButtonSystem
} // namespace WeirdEngine
