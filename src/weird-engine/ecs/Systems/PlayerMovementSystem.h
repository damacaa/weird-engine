#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"


namespace ECS
{
	class PlayerMovementSystem : public System
	{
	private:

		// Prevents the camera from jumping around when first clicking left click
		bool firstClick = true;
		bool m_locked = false;

		// Adjust the speed of the camera and it's sensitivity when looking around
		float speed = 10.1f;
		float sensitivity = 50000.0f;

	public:

		PlayerMovementSystem(ECSManager& ecs)
		{

		}

		void update(ECSManager& ecs, Entity target, float delta)
		{
			Transform& t = ecs.getComponent<Transform>(target);
			Camera& c = ecs.getComponent<Camera>(target);

			uint32_t m_width = 1200;
			uint32_t m_height = 800;

			// Handles key inputs
			if (Input::GetKey(Input::W))
			{
				t.position += delta * speed * c.camera.Orientation;
			}
			if (Input::GetKey(Input::A))
			{
				t.position += delta * speed * -glm::normalize(glm::cross(t.rotation, c.camera.Up));
			}
			if (Input::GetKey(Input::S))
			{
				t.position += delta * speed * -t.rotation;
			}
			if (Input::GetKey(Input::D))
			{
				t.position += delta * speed * glm::normalize(glm::cross(t.rotation, c.camera.Up));
			}
			if (Input::GetKey(Input::Space))
			{
				t.position += delta * speed * c.camera.Up;
			}
			if (Input::GetKey(Input::LeftCrtl))
			{
				t.position += delta * speed * -c.camera.Up;
			}

			if (Input::GetKeyDown(Input::LeftShift))
			{
				speed *= 4.0f;
			}
			else if (Input::GetKeyUp(Input::LeftShift))
			{
				speed *= 0.25f;
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
				float rotX = delta * sensitivity * (float)(mouseY - (m_height / 2)) / m_height;
				float rotY = delta * sensitivity * (float)(mouseX - (m_width / 2)) / m_width;

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

	private:


	};
}