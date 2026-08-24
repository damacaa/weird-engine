#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "CompiledMathExpressions.h"
#include "MathExpressions.h"
#include "Primitives.h"
#include "SDF.h"
#include "StarShape.h"

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace DefaultShapes
	{
		using namespace SDF;

		inline auto var(uint8_t index)
		{
			return std::make_shared<FloatVariable>(index);
		}

		inline const uint16_t CIRCLE = Scene::registerDefaultSDF(
			sdCircle(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), Expr(var(2))).node);

		inline const uint16_t CIRCLE_LINE = Scene::registerDefaultSDF(
			sdfOnion(sdCircle(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), Expr(var(2))), Expr(var(3))).node);

		inline const uint16_t BOX = Scene::registerDefaultSDF(
			sdBox(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), {Expr(var(2)), Expr(var(3))}).node);

		inline const uint16_t BOX_LINE = Scene::registerDefaultSDF(
			sdfOnion(sdBox(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), {Expr(var(2)), Expr(var(3))}),
					 Expr(var(4)))
				.node);

		inline const uint16_t TRIANGLE = Scene::registerDefaultSDF(
			sdTriangle(rotate(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), Expr(var(4))), Expr(var(2)),
					   Expr(var(3)))
				.node);

		inline const uint16_t TRIANGLE_LINE = Scene::registerDefaultSDF(
			sdfOnion(sdTriangle(rotate(translate(worldPoint(), {Expr(var(0)), Expr(var(1))}), Expr(var(4))),
								Expr(var(2)), Expr(var(3))),
					 Expr(var(5)))
				.node);

		inline const uint16_t LINE = Scene::registerDefaultSDF(
			sdLine(worldPoint(), {Expr(var(0)), Expr(var(1))}, {Expr(var(2)), Expr(var(3))}, Expr(var(4))).node);

		inline const uint16_t RAMP = Scene::registerDefaultSDF(std::make_shared<Primitives::Ramp>(
			var(Primitives::Ramp::POS_X), var(Primitives::Ramp::POS_Y), var(Primitives::Ramp::WIDTH),
			var(Primitives::Ramp::HEIGHT), var(Primitives::Ramp::SKEW)));

		inline const uint16_t SINE = Scene::registerDefaultSDF(std::make_shared<Primitives::SineWave>(
			var(Primitives::SineWave::AMPLITUDE), var(Primitives::SineWave::PERIOD), var(Primitives::SineWave::SPEED),
			var(Primitives::SineWave::OFFSET)));

		inline const uint16_t STAR = Scene::registerDefaultSDF(getStarShape());

	} // namespace DefaultShapes
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT2DSDFS_H
