#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct Light
		{
			uint32_t type = 0;
			glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
			uint32_t padding = 0; // could be something in the future
			glm::vec3 rotation = glm::vec3(0, 0.0f, 0);
			glm::vec4 color = glm::vec4(1.0f);
		};
	}
}