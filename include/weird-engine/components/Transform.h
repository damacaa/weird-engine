#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	namespace ECS
	{
		struct Transform
		{
			glm::vec3 position;
			glm::vec3 rotation;
			glm::vec3 scale = glm::vec3(1.0f);
		};
	} // namespace ECS
} // namespace WeirdEngine
