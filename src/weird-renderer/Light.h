#pragma once
#include <glm/glm.hpp>

struct Light
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 color = glm::vec3(1.0f);
};