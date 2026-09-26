#pragma once
#include "weird-engine/ColorPalette.h"
#include "weird-engine/vec.h"
#include <cstdint>
#include <string>

namespace WeirdEngine
{
	enum class Pattern2D : uint32_t
	{
		None = 0,
		Checkers = 1,
		PerlinNoise = 2,
		Waves = 3,
		Gradient = 4
	};

	struct Material2D
	{
		uint16_t id = 0;
		std::string name = "";

		vec4 color = vec4(1.0f); // Primary fill color

		// Currently unsupported. Would require rewritting the material blending shader
		vec4 secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f); // Secondary color for patterns/gradients
		Pattern2D pattern = Pattern2D::None;
		float patternScale = 1.0f;
	};

	struct Material2DHandle
	{
		uint16_t id = 0;
		constexpr Material2DHandle(uint16_t id = 0)
			: id(id)
		{
		}
		constexpr Material2DHandle(const Material2D& mat)
			: id(mat.id)
		{
		}
		constexpr operator uint16_t() const
		{
			return id;
		}
	};
} // namespace WeirdEngine
