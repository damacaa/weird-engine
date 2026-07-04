#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <array>

#include "weird-engine/vec.h"
#include "CompiledMathExpressions.h"
#include "MathExpressions.h"

namespace WeirdEngine
{
    struct ShapeMacro : IMathExpression
	{
	protected:
		static constexpr uint8_t VALUES_SIZE = 11;
		static constexpr uint8_t TIME = 8;
		static constexpr uint8_t WORLD_X = 9;
		static constexpr uint8_t WORLD_Y = 10;


	public:
		ShapeMacro() {}

		~ShapeMacro() override = default;

		[[nodiscard]]
		float getValue(const float* parameters) const override = 0;

		[[nodiscard]]
		std::string print() const override = 0;
	};
}
