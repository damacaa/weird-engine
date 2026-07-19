#pragma once
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	enum class MaterialPattern : uint32_t
	{
		None = 0,
		Checkers = 1,
		PerlinNoise = 2,
		Waves = 3
	};

	struct Material3D
	{
		uint16_t id = 0;
		vec4 color = vec4(1.0f);
		vec4 secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		float metallic = 0.0f;
		float roughness = 1.0f;
		MaterialPattern pattern = MaterialPattern::None;
		float patternScale = 1.0f;
	};
} // namespace WeirdEngine
