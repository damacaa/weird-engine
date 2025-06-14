#include "weird-renderer/Camera.h"
#include "weird-engine/Input.h"
#include <iomanip>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		Camera::Camera(glm::vec3 position)
		{
			position = position;
		}

		const auto RIGHT = glm::vec3(1.0f, 0.0f, 0.0f);
		const auto UP = glm::vec3(0.0f, 1.0f, 0.0f);
		const auto FORWARD = glm::vec3(0.0f, 0.0f, 1.0f);

		void Camera::updateMatrix(float nearPlane, float farPlane, int width, int height)
		{
			if (lookAtMode)
			{
				// Makes camera look in the right direction from the right position
				view = glm::lookAt(position, position + orientation, up);
			}
			else
			{
				view = glm::mat4(1.0f);

				view = glm::rotate(view, orientation.x, RIGHT);
				view = glm::rotate(view, orientation.y, UP);
				view = glm::rotate(view, orientation.z, FORWARD);
			}

			// Adds perspective to the Scene
			projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);

			// Sets new camera matrix
			cameraMatrix = projection * view;
		}
	}
}
