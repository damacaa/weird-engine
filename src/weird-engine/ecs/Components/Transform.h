#pragma once
#include "../Component.h"
#include <glm/glm.hpp>

// Example Component
struct Transform : public Component {
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale = glm::vec3(1.0f);
	bool isDirty = true;
};

