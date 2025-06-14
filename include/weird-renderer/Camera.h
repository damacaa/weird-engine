#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "Shader.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		class Camera
		{
		public:
			// Camera constructor to set up initial values
			Camera(glm::vec3 position);

			// Stores the main vectors of the camera
			glm::vec3 position;
			glm::vec3 orientation = glm::vec3(0.0f, 0.0f, -1.0f);
			glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

			glm::mat4 cameraMatrix;
			glm::mat4 projection;
			glm::mat4 view;

			float fov = 45.0f;
			bool lookAtMode = true;

			// Updates the camera matrix to the Vertex Shader
			void updateMatrix(float nearPlane, float farPlane, int width, int height);
		};
	}
}

#endif