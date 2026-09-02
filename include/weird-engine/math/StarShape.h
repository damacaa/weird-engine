#pragma once

#include "SDF.h"

namespace WeirdEngine
{
	inline Expr getStarShape()
	{
		using namespace SDF;
		auto p = translate(worldPoint(), {var(0), var(1)});
		auto radius = var(2);
		auto displacement = var(3);
		auto points = var(4);
		auto speed = var(5);

		return sdStar(p, radius, displacement, points, speed);
	}
} // namespace WeirdEngine
