#include"Camera.h"
#include "../weird-engine/Input.h"
#include <iomanip>


Camera::Camera(glm::vec3 position)
{

	Position = position;

	//instance = this;
}

void Camera::UpdateMatrix(float nearPlane, float farPlane, int width, int height)
{
	m_width = width;
	m_height = height;

	// Initializes matrices since otherwise they will be the null matrix
	view = glm::mat4(1.0f);
	projection = glm::mat4(1.0f);

	// Makes camera look in the right direction from the right position
	view = glm::lookAt(Position, Position + Orientation, Up);

	// Adds perspective to the Scene
	projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);

	// Sets new camera matrix
	cameraMatrix = projection * view;
}

void Camera::Matrix(Shader& shader, const char* uniform)
{
	// Exports camera matrix
	glUniformMatrix4fv(glGetUniformLocation(shader.ID, uniform), 1, GL_FALSE, glm::value_ptr(cameraMatrix));
}


void Camera::Inputs(float delta)
{
	// Handles key inputs
	if (Input::GetKey(Input::W))
	{
		Position += delta * speed * Orientation;
	}
	if (Input::GetKey(Input::A))
	{
		Position += delta * speed * -glm::normalize(glm::cross(Orientation, Up));
	}
	if (Input::GetKey(Input::S))
	{
		Position += delta * speed * -Orientation;
	}
	if (Input::GetKey(Input::D))
	{
		Position += delta * speed * glm::normalize(glm::cross(Orientation, Up));
	}
	if (Input::GetKey(Input::Space))
	{
		Position += delta * speed * Up;
	}
	if (Input::GetKey(Input::LeftCrtl))
	{
		Position += delta * speed * -Up;
	}

	if (Input::GetKey(Input::LeftShift))
	{
		speed = 0.4f;
	}
	else if (Input::GetKeyUp(Input::LeftCrtl))
	{
		speed = 0.1f;
	}

	if (Input::GetMouseButton(Input::WheelDown))
	{
		fov += delta * 3.0f;
	}
	else 	if (Input::GetMouseButton(Input::WheelUp))
	{
		fov -= delta * 2.0f;
	}


	// Handles mouse inputs
	if (Input::GetMouseButton(Input::LeftClick))
	{
		Input::HideMouse();

		// Prevents camera from jumping on the first click
		if (firstClick)
		{
			Input::SetMousePosition((m_width / 2), (m_height / 2));
			firstClick = false;
		}

		// Stores the coordinates of the cursor
		double mouseX = Input::GetMouseX();
		double mouseY = Input::GetMouseY();

		// Normalizes and shifts the coordinates of the cursor such that they begin in the middle of the screen
		// and then "transforms" them into degrees 
		float rotX = delta * sensitivity * (float)(mouseY - (m_height / 2)) / m_height;
		float rotY = delta * sensitivity * (float)(mouseX - (m_width / 2)) / m_width;

		// Calculates upcoming vertical change in the Orientation
		glm::vec3 newOrientation = glm::rotate(Orientation, glm::radians(-rotX), glm::normalize(glm::cross(Orientation, Up)));

		// Decides whether or not the next vertical Orientation is legal or not
		if (abs(glm::angle(newOrientation, Up) - glm::radians(90.0f)) <= glm::radians(85.0f))
		{
			Orientation = newOrientation;
		}

		if (rotY > 1) {
			int a = 0;
		}

		// Rotates the Orientation left and right
		Orientation = glm::rotate(Orientation, glm::radians(-rotY), Up);

		//std::cout << Orientation.x << " " << Orientation.y << " " << Orientation.z << " " << std::endl;

		// Sets mouse cursor to the middle of the screen so that it doesn't end up roaming around
		Input::SetMousePosition((m_width / 2), (m_height / 2));
	}
	else if (Input::GetMouseButtonUp(Input::LeftClick))
	{
		// Unhides cursor since camera is not looking around anymore
		Input::ShowMouse();
		// Makes sure the next time the camera looks around it doesn't jump
		firstClick = true;
	}
}

