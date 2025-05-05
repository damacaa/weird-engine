#include "weird-renderer/Camera.h"
#include "weird-engine/Input.h"
#include <iomanip>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
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
			/*view = glm::mat4(1.0f);
			projection = glm::mat4(1.0f);*/

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
			shader.setUniform(uniform, cameraMatrix);
			//glUniformMatrix4fv(glGetUniformLocation(shader.ID, uniform), 1, GL_FALSE, glm::value_ptr(cameraMatrix));
		}

	}
}
