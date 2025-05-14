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

		void Camera::updateMatrix(float nearPlane, float farPlane, int width, int height)
		{
			// Makes camera look in the right direction from the right position
			view = glm::lookAt(position, position + orientation, up);

			// Adds perspective to the Scene
			projection = glm::perspective(glm::radians(fov), (float)width / height, nearPlane, farPlane);

			// Sets new camera matrix
			cameraMatrix = projection * view;
		}
	}
}
