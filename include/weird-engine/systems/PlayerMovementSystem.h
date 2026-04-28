#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"

#include <algorithm>
#include <cmath>

namespace WeirdEngine
{
	namespace ECS
	{
		namespace PlayerMovementSystem
		{

			inline bool m_locked2D = false;
			inline bool m_locked3D = false;
			inline bool m_skipMouseDelta2D = false;
			inline bool m_skipMouseDelta3D = false;
			inline constexpr float MAX_DEBUG_CAMERA_DELTA = 1.0f / 30.0f;

			inline float clampMovementDelta(float delta)
			{
				return std::clamp(delta, 0.0f, MAX_DEBUG_CAMERA_DELTA);
			}

			inline void updateMovement2D(ECSManager& ecs, float delta);
			inline void updateFly(ECSManager& ecs, float delta);

			inline void update(ECSManager& ecs, float delta)
			{
				updateMovement2D(ecs, delta);
				updateFly(ecs, delta);
			}

			inline void updateMovement2D(ECSManager& ecs, float delta)
			{
				float safeDelta = clampMovementDelta(delta);

				auto componentArray = ecs.getComponentManager<FlyMovement2D>()->getComponentArray();
				unsigned int size = componentArray->getSize();
				for (size_t i = 0; i < size; i++)
				{

					auto& flyComponent = componentArray->getDataAtIdx(i);
					Entity target = flyComponent.Owner;

					Transform& t = ecs.getComponent<Transform>(target);
					Camera& c = ecs.getComponent<Camera>(target);

					vec3 targetPosition = flyComponent.targetPosition;

					if (Input::GetMouseButtonDown(Input::MiddleClick) ||
						(Input::GetMouseButton(Input::MiddleClick) && !m_locked2D))
					{
						m_locked2D = true;
						m_skipMouseDelta2D = true;
						Input::HideMouse();
					}
					else if (Input::GetMouseButtonUp(Input::MiddleClick))
					{
						m_locked2D = false;
						Input::ShowMouse();
					}

					if (m_locked2D)
					{
						if (m_skipMouseDelta2D)
						{
							m_skipMouseDelta2D = false;
						}
						else
						{
							float zoom = std::max(t.position.z, 5.0f);
							float worldUnitsPerPixel =
								zoom / (0.5f * static_cast<float>(std::max(1, WeirdRenderer::Display::height)));
							targetPosition += flyComponent.speed *
										  vec3(-Input::GetMouseDeltaXRaw() * worldUnitsPerPixel,
											   Input::GetMouseDeltaYRaw() * worldUnitsPerPixel, 0.0f);
						}
					}

					if (Input::GetMouseButtonDown(Input::LeftClick))
					{
					}

					// Handles key inputs
					if (!Input::GetKey(Input::LeftCtrl))
					{
						if (Input::GetKey(Input::W))
						{
							targetPosition += t.position.z * safeDelta * flyComponent.speed * c.camera.up;
						}
						if (Input::GetKey(Input::A))
						{
							targetPosition += t.position.z * safeDelta * flyComponent.speed *
											  -glm::normalize(glm::cross(t.rotation, c.camera.up));
						}
						if (Input::GetKey(Input::S))
						{
							targetPosition += t.position.z * safeDelta * flyComponent.speed * -c.camera.up;
						}
						if (Input::GetKey(Input::D))
						{
							targetPosition += t.position.z * safeDelta * flyComponent.speed *
											  glm::normalize(glm::cross(t.rotation, c.camera.up));
						}
					}

					if (Input::GetMouseButton(Input::WheelDown))
					{
						// Move camera backwards
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed * -t.rotation;

						// Move camera towards the cursor
						vec2 cursorPosition =
							Camera::screenPositionToWorldPosition2D(t, vec2(Input::GetMouseX(), Input::GetMouseY()));
						vec2 travel = cursorPosition - (vec2)t.position;
						float safeZoom = std::max(std::abs(t.position.z), 0.001f);
						targetPosition -= flyComponent.scrollSpeed * flyComponent.speed *
										  vec3(travel.x / safeZoom, travel.y / safeZoom, 0);
					}
					else if (Input::GetMouseButton(Input::WheelUp))
					{
						// Move camera forwards
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed * t.rotation;

						// Move camera towards the cursor
						vec2 cursorPosition =
							Camera::screenPositionToWorldPosition2D(t, vec2(Input::GetMouseX(), Input::GetMouseY()));
						vec2 travel = cursorPosition - (vec2)t.position;
						float safeZoom = std::max(std::abs(t.position.z), 0.001f);
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed *
										  vec3(travel.x / safeZoom, travel.y / safeZoom, 0);
					}

					if (targetPosition.z < 5.0f)
					{
						targetPosition.z = 5.0f;
					}

					if (Input::GetKeyDown(Input::Space))
					{
						flyComponent.speed *= 4.0f;
					}
					else if (Input::GetKeyUp(Input::Space))
					{
						flyComponent.speed *= 0.25f;
					}

					flyComponent.targetPosition = targetPosition;

					if (flyComponent.isSmooth)
					{
						// Exponential smoothing is frame-rate independent and resilient to occasional large frame times.
						float smoothing = 14.0f;
						float alpha = 1.0f - std::exp(-smoothing * safeDelta);
						t.position = glm::mix(t.position, flyComponent.targetPosition, alpha);
					}
					else
					{
						t.position = targetPosition;
					}
				}
			}

			inline void updateFly(ECSManager& ecs, float delta)
			{
				float safeDelta = clampMovementDelta(delta);

				auto& componentArray = *ecs.getComponentManager<FlyMovement>()->getComponentArray();
				unsigned int size = componentArray.getSize();
				for (size_t i = 0; i < size; i++)
				{
					auto& flyComponent = componentArray[i];
					Entity target = flyComponent.Owner;

					Transform& t = ecs.getComponent<Transform>(target);
					Camera& c = ecs.getComponent<Camera>(target);
					glm::vec3 forward = glm::normalize(t.rotation);
					glm::vec3 right = glm::normalize(glm::cross(forward, c.camera.up));

					// Handles key inputs
					if (Input::GetKey(Input::W))
					{
						t.position += safeDelta * flyComponent.speed * forward;
					}
					if (Input::GetKey(Input::A))
					{
						t.position += safeDelta * flyComponent.speed * -right;
					}
					if (Input::GetKey(Input::S))
					{
						t.position += safeDelta * flyComponent.speed * -forward;
					}
					if (Input::GetKey(Input::D))
					{
						t.position += safeDelta * flyComponent.speed * right;
					}
					if (Input::GetKey(Input::Space))
					{
						t.position += safeDelta * flyComponent.speed * c.camera.up;
					}
					if (Input::GetKey(Input::LeftCtrl))
					{
						t.position += safeDelta * flyComponent.speed * -c.camera.up;
					}

					if (Input::GetKeyDown(Input::LeftShift))
					{
						flyComponent.speed *= 4.0f;
					}
					else if (Input::GetKeyUp(Input::LeftShift))
					{
						flyComponent.speed *= 0.25f;
					}

					if (Input::GetMouseButton(Input::WheelDown))
					{
						c.camera.fov = std::min(120.0f, c.camera.fov + 2.0f);
					}
					else if (Input::GetMouseButton(Input::WheelUp))
					{
						c.camera.fov = std::max(20.0f, c.camera.fov - 2.0f);
					}

					if (Input::GetKey(Input::KeyCode::Esc))
					{
						m_locked3D = false;
						// Unhides cursor since camera is not looking around anymore
						Input::ShowMouse();
						m_skipMouseDelta3D = true;
					}

					if (Input::GetMouseButtonDown(Input::LeftClick) ||
						(Input::GetMouseButton(Input::LeftClick) && !m_locked3D))
					{
						m_locked3D = true;
						m_skipMouseDelta3D = true;
						Input::HideMouse();
					}

					if (m_locked3D)
					{
						if (m_skipMouseDelta3D)
						{
							m_skipMouseDelta3D = false;
							continue;
						}

						// Relative mouse deltas already include frame-time; do not multiply by delta.
						float fovMultiplier = c.camera.fov / 90.0f;
						float rotX = fovMultiplier * flyComponent.sensitivity * Input::GetMouseDeltaYRaw();
						float rotY = fovMultiplier * flyComponent.sensitivity * Input::GetMouseDeltaXRaw();

						// Calculates upcoming vertical change in the Orientation
						glm::vec3 newOrientation = glm::rotate(t.rotation, glm::radians(-rotX), right);
						newOrientation = glm::normalize(newOrientation);

						// Decides whether or not the next vertical Orientation is legal or not
						if (std::abs(glm::angle(newOrientation, c.camera.up) - glm::radians(90.0f)) <=
							glm::radians(85.0f))
						{
							t.rotation = newOrientation;
						}

						// Rotates the Orientation left and right
						t.rotation = glm::rotate(t.rotation, glm::radians(-rotY), c.camera.up);
						t.rotation = glm::normalize(t.rotation);
					}
				}
			}

		} // namespace PlayerMovementSystem
	} // namespace ECS
} // namespace WeirdEngine