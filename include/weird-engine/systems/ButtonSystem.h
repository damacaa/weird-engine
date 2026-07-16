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
			bool mouseIsClicking = Input::GetMouseButton(Input::LeftClick);

			ecs.forEach<ShapeButton, UIShape>(
				[&](Entity buttonOwner, ShapeButton& buttonComponent, UIShape& shape)
				{
					{
						float parameters[11];

						std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

						parameters[8] = time;
						parameters[9] = Input::GetMouseX();
						parameters[10] = Input::GetMouseY();

						float distance = sdfs[shape.distanceFieldId]->getValue(parameters);
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
				});
		}

		inline void updateToggles(ECSManager& ecs, std::vector<std::shared_ptr<IMathExpression>>& sdfs, float time)
		{
			bool mouseIsClicking = Input::GetMouseButtonDown(Input::LeftClick);

			ecs.forEach<ShapeToggle, UIShape>(
				[&](Entity toggleOwner, ShapeToggle& toggleComponent, UIShape& shape)
				{
					{
						float parameters[11];

						std::copy(std::begin(shape.parameters), std::end(shape.parameters), std::begin(parameters));

						parameters[8] = time;
						parameters[9] = Input::GetMouseX();
						parameters[10] = Input::GetMouseY();

						float distance = sdfs[shape.distanceFieldId]->getValue(parameters);
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
				});
		}
	} // namespace ButtonSystem
} // namespace WeirdEngine
