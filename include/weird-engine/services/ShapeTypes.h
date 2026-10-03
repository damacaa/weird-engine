#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

#include "weird-engine/Material2D.h"
#include "weird-engine/Material3D.h"
#include "weird-renderer/components/Shape.h"

namespace WeirdEngine
{
	struct ShapeParamValue
	{
		size_t index = 0;
		float value = 0.0f;
	};

	struct ShapeVariables
	{
		float data[8]{};

		constexpr ShapeVariables() = default;

		constexpr ShapeVariables(std::initializer_list<float> list)
		{
			size_t i = 0;
			for (float v : list)
			{
				if (i >= 8)
					break;
				data[i++] = v;
			}
		}

		constexpr ShapeVariables(std::initializer_list<ShapeParamValue> indexedList)
		{
			for (const auto& pv : indexedList)
			{
				if (pv.index < 8)
				{
					data[pv.index] = pv.value;
				}
			}
		}

		template <size_t N> constexpr ShapeVariables(const float (&arr)[N])
		{
			const size_t count = std::min<size_t>(N, 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = arr[i];
		}

		constexpr ShapeVariables(std::span<const float> s)
		{
			const size_t count = std::min<size_t>(s.size(), 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = s[i];
		}

		constexpr ShapeVariables(const float* ptr, size_t n = 8)
		{
			const size_t count = std::min<size_t>(n, 8);
			for (size_t i = 0; i < count; ++i)
				data[i] = ptr[i];
		}
	};

	struct ShapeMaterial
	{
		uint16_t id = 0;

		constexpr ShapeMaterial() = default;
		constexpr ShapeMaterial(uint16_t matId)
			: id(matId)
		{
		}
		constexpr ShapeMaterial(int matId)
			: id(static_cast<uint16_t>(matId))
		{
		}
		ShapeMaterial(const Material2D& mat)
			: id(mat.id)
		{
		}
		ShapeMaterial(const Material3D& mat)
			: id(mat.id)
		{
		}
		constexpr ShapeMaterial(Material2DHandle handle)
			: id(handle.id)
		{
		}
		constexpr ShapeMaterial(Material3DHandle handle)
			: id(handle.id)
		{
		}
		constexpr operator uint16_t() const
		{
			return id;
		}
	};

	struct ShapeConfig
	{
		ShapeId shapeId = 0;
		ShapeVariables variables{};
		ShapeMaterial material = 0;
		CombinationType combination = CombinationType::Addition;
		bool hasCollision = true;
		int group = 0;
	};

	struct UIShapeConfig
	{
		ShapeId shapeId = 0;
		ShapeVariables variables{};
		ShapeMaterial material = 0;
		CombinationType combination = CombinationType::Addition;
		int group = 0;
	};
} // namespace WeirdEngine
