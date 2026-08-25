#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	enum class Light3DType : uint32_t
	{
		Directional = 0,
		Point = 1,
		Spot = 2
	};

	struct Light3DComponent
	{
		Light3DType type = Light3DType::Directional;
		glm::vec4 color = glm::vec4(1.0f);
		float radius = 25.0f;
		float spotAngle = 45.0f;
	};

	// Keep LightComponent as an alias for Light3DComponent if any 3D code includes it
	using LightType = Light3DType;
	using LightComponent = Light3DComponent;
} // namespace WeirdEngine
