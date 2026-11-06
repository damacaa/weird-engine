#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	enum class Light2DType : uint32_t
	{
		Directional = 0,
		Point = 1,
		Cone = 2
	};

	struct Light2DComponent
	{
		Light2DType type = Light2DType::Directional;
		glm::vec4 color = glm::vec4(1.0f);				   // RGB = color, A = intensity
		glm::vec2 direction = glm::vec2(0.7071f, 0.7071f); // Direction for Directional and Cone lights
		float radius = 10.0f;							   // Max attenuation radius (Point & Cone)
		float coneAngle = 45.0f;						   // Cone angle in degrees (Cone lights)
		float conePenumbra = 10.0f;						   // Soft penumbra angle in degrees (Cone lights)
		bool castShadows = true;
	};
} // namespace WeirdEngine
