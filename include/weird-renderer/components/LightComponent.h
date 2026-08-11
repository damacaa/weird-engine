#pragma once
#include <glm/glm.hpp>

namespace WeirdEngine
{
	enum class LightType : uint32_t
	{
		Directional = 0,
		Point = 1,
		Spot = 2
	};

	struct LightComponent
	{
		LightType type = LightType::Directional;
		glm::vec4 color = glm::vec4(1.0f);
	};
} // namespace WeirdEngine
