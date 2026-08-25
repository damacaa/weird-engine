#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct Light2D
		{
			uint32_t type = 0; // 0 = Directional, 1 = Point, 2 = Cone
			glm::vec2 position = glm::vec2(0.0f);
			glm::vec2 direction = glm::vec2(0.7071f, 0.7071f);
			glm::vec4 color = glm::vec4(1.0f); // RGB = color, A = intensity multiplier
			float radius = 10.0f;
			float coneAngle = 0.785398f;	// Radians (45 deg)
			float conePenumbra = 0.174533f; // Radians (10 deg)
			uint32_t castShadows = 1;
		};

		struct Light3D
		{
			uint32_t type = 0; // 0 = Directional, 1 = Point, 2 = Spot
			glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
			uint32_t padding = 0;
			glm::vec3 rotation = glm::vec3(0.0f, 0.0f, 0.0f);
			glm::vec4 color = glm::vec4(1.0f);
		};

		using Light = Light3D;
	} // namespace WeirdRenderer
} // namespace WeirdEngine