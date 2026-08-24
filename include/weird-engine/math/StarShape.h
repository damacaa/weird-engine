#pragma once

#include "MathExpressions.h"
#include "SDF.h"

namespace WeirdEngine
{
	inline std::shared_ptr<IMathExpression> getStarShape()
	{
		using namespace SDF;
		auto p = translate(worldPoint(), {Expr(var(0)), Expr(var(1))});
		auto radius = Expr(var(2));
		auto displacement = Expr(var(3));
		auto points = Expr(var(4));
		auto speed = Expr(var(5));

		return sdStar(p, radius, displacement, points, speed).node;
	}
} // namespace WeirdEngine
