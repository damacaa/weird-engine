#pragma once
#include "../Component.h"
#include <glm/glm.hpp>

namespace WeirdEngine
{
	namespace ECS
	{
		struct Transform : public Component {
			glm::vec3 position;
			glm::vec3 rotation;
			glm::vec3 scale = glm::vec3(1.0f);
			bool isDirty = true;
		};
	}
}

