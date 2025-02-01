#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct Light
		{
			glm::vec3 position = glm::vec3(10.5f, 0.5f, 0.5f);
			glm::vec3 rotation = glm::vec3(0.0f);
			glm::vec4 color = glm::vec4(1.0f);
		};
	}
}