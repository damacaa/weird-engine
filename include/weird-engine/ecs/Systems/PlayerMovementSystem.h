#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"

namespace WeirdEngine
{
	namespace ECS
	{
		class PlayerMovementSystem : public System
		{
		private:

			// Prevents the camera from jumping around when first clicking left click
			bool firstClick = true;
			bool m_locked = false;

			// Adjust the speed of the camera and it's sensitivity when looking around



		public:

			PlayerMovementSystem(ECSManager& ecs)
			{

			}

			void update(ECSManager& ecs, float delta)
			{
				updateMovement2D(ecs, delta);
				updateFly(ecs, delta);
			}

		private:



			void updateMovement2D(ECSManager& ecs, float delta)
			{
				auto& componentArray = *ecs.getComponentManager<FlyMovement2D>()->getComponentArray<FlyMovement2D>();
				unsigned int size = componentArray.getSize();
				for (size_t i = 0; i < size; i++)
				{


					auto& flyComponent = componentArray[i];
					Entity target = flyComponent.Owner;

					Transform& t = ecs.getComponent<Transform>(target);
					Camera& c = ecs.getComponent<Camera>(target);

					uint32_t m_width = 1200; // Todo get real screen size
					uint32_t m_height = 800;

					vec3 targetPosition = flyComponent.targetPosition;

					if (Input::GetMouseButtonDown(Input::MiddleClick))
					{
						firstClick = true;
						m_locked = true;
						Input::HideMouse();
					}
					else if (Input::GetMouseButtonUp(Input::MiddleClick))
					{
						m_locked = false;
						Input::ShowMouse();
					}

					if (firstClick)
					{
						firstClick = false;
					}
					else if (m_locked)
					{
						targetPosition += 100.0f * t.position.z * delta * flyComponent.speed * vec3(-Input::GetMouseDeltaX(), Input::GetMouseDeltaY(), 0.f);
						Input::SetMousePosition((m_width / 2), (m_height / 2));
					}

					if (Input::GetMouseButtonDown(Input::LeftClick))
					{

					}

					// Handles key inputs
					if (Input::GetKey(Input::W))
					{
						targetPosition += t.position.z * delta * flyComponent.speed * c.camera.Up;
					}
					if (Input::GetKey(Input::A))
					{
						targetPosition += t.position.z * delta * flyComponent.speed * -glm::normalize(glm::cross(t.rotation, c.camera.Up));
					}
					if (Input::GetKey(Input::S))
					{
						targetPosition += t.position.z * delta * flyComponent.speed * -c.camera.Up;
					}
					if (Input::GetKey(Input::D))
					{
						targetPosition += t.position.z * delta * flyComponent.speed * glm::normalize(glm::cross(t.rotation, c.camera.Up));
					}


					if (Input::GetMouseButton(Input::WheelDown))
					{
						// Move camera backwards
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed * -t.rotation;

						// Move camera towards the cursor
						vec2 cursorPosition = Camera::screenPositionToWorldPosition2D(t, vec2(Input::GetMouseX(), Input::GetMouseY()));
						vec2 travel = cursorPosition - (vec2)t.position;
						targetPosition -= flyComponent.scrollSpeed * flyComponent.speed * vec3(travel.x / abs(t.position.z), travel.y / abs(t.position.z), 0);
					}
					else if (Input::GetMouseButton(Input::WheelUp))
					{
						// Move camera forwards
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed * c.camera.Orientation;

						// Move camera towards the cursor
						vec2 cursorPosition = Camera::screenPositionToWorldPosition2D(t, vec2(Input::GetMouseX(), Input::GetMouseY()));
						vec2 travel = cursorPosition - (vec2)t.position;
						targetPosition += flyComponent.scrollSpeed * flyComponent.speed * vec3(travel.x / abs(t.position.z), travel.y / abs(t.position.z), 0);
					}

					if (targetPosition.z < 20.0f)
					{
						targetPosition.z = 20.0f;
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
						vec3 a = flyComponent.targetPosition - t.position;
						flyComponent.v += (500.0f * delta * a) - (0.5f * flyComponent.v);
						t.position += delta * flyComponent.v;
					} 
					else
					{
						t.position = targetPosition;
					}
				}
			}

			void updateFly(ECSManager& ecs, float delta)
			{
				auto& componentArray = *ecs.getComponentManager<FlyMovement>()->getComponentArray<FlyMovement>();
				unsigned int size = componentArray.getSize();
				for (size_t i = 0; i < size; i++)
				{
					auto& flyComponent = componentArray[i];
					Entity target = flyComponent.Owner;

					Transform& t = ecs.getComponent<Transform>(target);
					Camera& c = ecs.getComponent<Camera>(target);

					uint32_t m_width = 1200;
					uint32_t m_height = 800;

					// Handles key inputs
					if (Input::GetKey(Input::W))
					{
						t.position += delta * flyComponent.speed * c.camera.Orientation;
					}
					if (Input::GetKey(Input::A))
					{
						t.position += delta * flyComponent.speed * -glm::normalize(glm::cross(t.rotation, c.camera.Up));
					}
					if (Input::GetKey(Input::S))
					{
						t.position += delta * flyComponent.speed * -t.rotation;
					}
					if (Input::GetKey(Input::D))
					{
						t.position += delta * flyComponent.speed * glm::normalize(glm::cross(t.rotation, c.camera.Up));
					}
					if (Input::GetKey(Input::Space))
					{
						t.position += delta * flyComponent.speed * c.camera.Up;
					}
					if (Input::GetKey(Input::LeftCrtl))
					{
						t.position += delta * flyComponent.speed * -c.camera.Up;
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
						c.camera.fov += delta * 300.0f;
					}
					else if (Input::GetMouseButton(Input::WheelUp))
					{
						c.camera.fov -= delta * 200.0f;
					}

					if (Input::GetKey(Input::KeyCode::Esc))
					{
						m_locked = false;
						// Unhides cursor since camera is not looking around anymore
						Input::ShowMouse();
						// Makes sure the next time the camera looks around it doesn't jump
						firstClick = true;
					}


					if (Input::GetMouseButton(Input::LeftClick)) {
						m_locked = true;
						Input::HideMouse();
					}


					if (m_locked) {

						// Stores the coordinates of the cursor
						double mouseX = Input::GetMouseX();
						double mouseY = Input::GetMouseY();

						// Prevents camera from jumping on the first click
						if (firstClick)
						{
							Input::SetMousePosition((m_width / 2), (m_height / 2));
							firstClick = false;
							mouseX = m_width / 2;
							mouseY = m_height / 2;
						}


						// Normalizes and shifts the coordinates of the cursor such that they begin in the middle of the screen
						// and then "transforms" them into degrees 
						float rotX = delta * flyComponent.sensitivity * (float)(mouseY - (m_height / 2)) / m_height;
						float rotY = delta * flyComponent.sensitivity * (float)(mouseX - (m_width / 2)) / m_width;

						// Calculates upcoming vertical change in the Orientation
						glm::vec3 newOrientation = glm::rotate(t.rotation, glm::radians(-rotX), glm::normalize(glm::cross(t.rotation, c.camera.Up)));

						// Decides whether or not the next vertical Orientation is legal or not
						if (abs(glm::angle(newOrientation, c.camera.Up) - glm::radians(90.0f)) <= glm::radians(85.0f))
						{
							t.rotation = newOrientation;
						}

						if (rotY > 1) {
							int a = 0;
						}

						// Rotates the Orientation left and right
						t.rotation = glm::rotate(t.rotation, glm::radians(-rotY), c.camera.Up);

						//std::cout << Orientation.x << " " << Orientation.y << " " << Orientation.z << " " << std::endl;

						// Sets mouse cursor to the middle of the screen so that it doesn't end up roaming around
						Input::SetMousePosition((m_width / 2), (m_height / 2));
					}
				}
			}

		};
	}
}