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
#include "StarShape.h"

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace DefaultShapes
	{
		inline auto var(uint8_t index)
		{
			return std::make_shared<FloatVariable>(index);
		}

		inline const uint16_t CIRCLE = Scene::registerDefaultSDF(std::make_shared<Primitives::Circle>(
			var(Primitives::Circle::POS_X), var(Primitives::Circle::POS_Y), var(Primitives::Circle::RADIUS)));

		inline const uint16_t CIRCLE_LINE = Scene::registerDefaultSDF(std::make_shared<SDFOnion>(
			std::make_shared<Primitives::Circle>(var(Primitives::Circle::POS_X), var(Primitives::Circle::POS_Y),
												 var(Primitives::Circle::RADIUS)),
			var(Primitives::Circle::RADIUS + 1)));

		inline const uint16_t BOX = Scene::registerDefaultSDF(
			std::make_shared<Primitives::Box>(var(Primitives::Box::POS_X), var(Primitives::Box::POS_Y),
											  var(Primitives::Box::SIZE_X), var(Primitives::Box::SIZE_Y)));

		inline const uint16_t BOX_LINE = Scene::registerDefaultSDF(std::make_shared<SDFOnion>(
			std::make_shared<Primitives::Box>(var(Primitives::Box::POS_X), var(Primitives::Box::POS_Y),
											  var(Primitives::Box::SIZE_X), var(Primitives::Box::SIZE_Y)),
			var(Primitives::Box::SIZE_Y + 1)));

		inline const uint16_t TRIANGLE = Scene::registerDefaultSDF(std::make_shared<Primitives::Triangle>(
			var(Primitives::Triangle::POS_X), var(Primitives::Triangle::POS_Y), var(Primitives::Triangle::SIZE_X),
			var(Primitives::Triangle::SIZE_Y), var(Primitives::Triangle::ROTATION)));

		inline const uint16_t TRIANGLE_LINE = Scene::registerDefaultSDF(std::make_shared<SDFOnion>(
			std::make_shared<Primitives::Triangle>(var(Primitives::Triangle::POS_X), var(Primitives::Triangle::POS_Y),
												   var(Primitives::Triangle::SIZE_X), var(Primitives::Triangle::SIZE_Y),
												   var(Primitives::Triangle::ROTATION)),
			var(Primitives::Triangle::ROTATION + 1)));

		inline const uint16_t LINE = Scene::registerDefaultSDF(std::make_shared<Primitives::Line>(
			var(Primitives::Line::POS_A_X), var(Primitives::Line::POS_A_Y), var(Primitives::Line::POS_B_X),
			var(Primitives::Line::POS_B_Y), var(Primitives::Line::WIDTH)));

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