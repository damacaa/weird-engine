#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <cstdint>

#include "Primitives.h"
#include "SDF.h"
#include "StarShape.h"

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace DefaultShapes
	{
		using namespace SDF;

		namespace Circle
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t RADIUS = 2;
		} // namespace Circle

		namespace CircleLine
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t RADIUS = 2;
			constexpr uint8_t THICKNESS = 3;
		} // namespace CircleLine

		namespace Box
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t SIZE_X = 2;
			constexpr uint8_t SIZE_Y = 3;
		} // namespace Box

		namespace BoxLine
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t SIZE_X = 2;
			constexpr uint8_t SIZE_Y = 3;
			constexpr uint8_t THICKNESS = 4;
		} // namespace BoxLine

		namespace Triangle
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t WIDTH = 2;
			constexpr uint8_t HEIGHT = 3;
		} // namespace Triangle

		namespace TriangleLine
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t WIDTH = 2;
			constexpr uint8_t HEIGHT = 3;
			constexpr uint8_t THICKNESS = 4;
		} // namespace TriangleLine

		namespace Line
		{
			constexpr uint8_t START_X = 0;
			constexpr uint8_t START_Y = 1;
			constexpr uint8_t END_X = 2;
			constexpr uint8_t END_Y = 3;
			constexpr uint8_t WIDTH = 4;
		} // namespace Line

		namespace Ramp
		{
			constexpr uint8_t POS_X = 0;
			constexpr uint8_t POS_Y = 1;
			constexpr uint8_t WIDTH = 2;
			constexpr uint8_t HEIGHT = 3;
			constexpr uint8_t SKEW = 4;
		} // namespace Ramp

		namespace SineWave
		{
			constexpr uint8_t AMPLITUDE = 0;
			constexpr uint8_t PERIOD = 1;
			constexpr uint8_t SPEED = 2;
			constexpr uint8_t OFFSET = 3;
		} // namespace SineWave

		inline const uint16_t CIRCLE =
			Scene::registerDefaultSDF(sdCircle(translate(worldPoint(), {var(0), var(1)}), var(2)));

		inline const uint16_t CIRCLE_LINE =
			Scene::registerDefaultSDF(sdfOnion(sdCircle(translate(worldPoint(), {var(0), var(1)}), var(2)), var(3)));

		inline const uint16_t BOX =
			Scene::registerDefaultSDF(sdBox(translate(worldPoint(), {var(0), var(1)}), {var(2), var(3)}));

		inline const uint16_t BOX_LINE = Scene::registerDefaultSDF(
			sdfOnion(sdBox(translate(worldPoint(), {var(0), var(1)}), {var(2), var(3)}), var(4)));

		inline const uint16_t TRIANGLE =
			Scene::registerDefaultSDF(sdTriangle(translate(worldPoint(), {var(0), var(1)}), var(2), var(3)));

		inline const uint16_t TRIANGLE_LINE = Scene::registerDefaultSDF(
			sdfOnion(sdTriangle(translate(worldPoint(), {var(0), var(1)}), var(2), var(3)), var(4)));

		inline const uint16_t LINE =
			Scene::registerDefaultSDF(sdLine(worldPoint(), {var(0), var(1)}, {var(2), var(3)}, var(4)));

		inline const uint16_t RAMP =
			Scene::registerDefaultSDF(sdRamp(translate(worldPoint(), {var(0), var(1)}), var(2), var(3), var(4)));

		inline const uint16_t SINE =
			Scene::registerDefaultSDF(sdSineWave(worldPoint(), var(0), var(1), var(2), var(3)));

		inline const uint16_t STAR = Scene::registerDefaultSDF(getStarShape());

		inline const uint16_t BOX_ROTATED = Scene::registerDefaultSDF(
			sdBox(rotate(translate(worldPoint(), {var(0), var(1)}), var(4)), {var(2), var(3)}));

		inline const uint16_t BOX_LINE_ROTATED = Scene::registerDefaultSDF(
			sdfOnion(sdBox(rotate(translate(worldPoint(), {var(0), var(1)}), var(4)), {var(2), var(3)}), var(5)));

		inline const uint16_t TRIANGLE_ROTATED = Scene::registerDefaultSDF(
			sdTriangle(rotate(translate(worldPoint(), {var(0), var(1)}), var(4)), var(2), var(3)));

		inline const uint16_t TRIANGLE_LINE_ROTATED = Scene::registerDefaultSDF(
			sdfOnion(sdTriangle(rotate(translate(worldPoint(), {var(0), var(1)}), var(4)), var(2), var(3)), var(5)));

		inline const uint16_t RAMP_ROTATED = Scene::registerDefaultSDF(
			sdRamp(rotate(translate(worldPoint(), {var(0), var(1)}), var(5)), var(2), var(3), var(4)));

	} // namespace DefaultShapes
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT2DSDFS_H
