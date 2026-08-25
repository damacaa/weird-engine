#pragma once
#include "weird-engine/ColorPalette.h"
#include "weird-engine/vec.h"
#include <cstdint>
#include <string>

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
		std::string name = "";

		vec4 color = vec4(1.0f);
		vec4 secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		float metallic = 0.0f;
		float roughness = 1.0f;
		MaterialPattern pattern = MaterialPattern::None;
		float patternScale = 1.0f;
		float emission = 0.0f;
	};

	struct Material3DHandle
	{
		uint16_t id = 0;
		constexpr Material3DHandle(uint16_t id = 0)
			: id(id)
		{
		}
		constexpr Material3DHandle(const Material3D& mat)
			: id(mat.id)
		{
		}
		constexpr operator uint16_t() const
		{
			return id;
		}
	};
} // namespace WeirdEngine
