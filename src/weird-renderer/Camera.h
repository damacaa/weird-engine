#ifndef CAMERA_CLASS_H
#define CAMERA_CLASS_H

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/type_ptr.hpp>
#include<glm/gtx/rotate_vector.hpp>
#include<glm/gtx/vector_angle.hpp>

#include <sstream>
#include <iomanip>
#include <iostream>

#include"Shader.h"

namespace WeirdRenderer
{
	class Camera
	{
	public:

		static Camera* instance;

		// Stores the main vectors of the m_camera
		glm::vec3 Position;
		glm::vec3 Orientation = glm::vec3(0.0f, 0.0f, -1.0f);
		glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 cameraMatrix = glm::mat4(1.0f);
		glm::mat4 projection;

		glm::mat4 view;

		float fov = 45.0f;



		// Camera constructor to set up initial values
		Camera(glm::vec3 position);

		// Updates the m_camera matrix to the Vertex Shader
		void UpdateMatrix(float nearPlane, float farPlane, int width, int height);
		// Exports the m_camera matrix to a shader
		void Matrix(Shader& shader, const char* uniform);

	private:

		unsigned int m_width, m_height;

		//void scroll_callback(GLFWwindow* m_window, double xoffset, double yoffset);

		static void DebugMat(glm::mat4 debugMat) {
			std::stringstream ss;
			for (size_t i = 0; i < 4; i++)
			{
				if (i == 0)
					ss << "[ ";
				else
					ss << "  ";

				for (size_t j = 0; j < 4; j++)
				{
					float value = debugMat[i][j];
					if (value >= 0)
						ss << " ";

					ss << std::fixed << std::setprecision(1) << value + 0.001f << " ";

				}

				if (i == 3)
					ss << "]";
				else
					ss << "\n";
			}

			std::cout << ss.str() << std::endl;
		}
	};
}

#endif