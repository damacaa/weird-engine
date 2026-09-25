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

#define WEIRD_BUILTIN_SHAPES_2D(X)                                                                                     \
	X(CIRCLE, Circle, (POS_X, POS_Y, RADIUS),                                                                          \
	  sdCircle(translate(point(), {var(Circle::POS_X), var(Circle::POS_Y)}), var(Circle::RADIUS)))                     \
	X(CIRCLE_LINE, CircleLine, (POS_X, POS_Y, RADIUS, THICKNESS),                                                      \
	  sdfOnion(                                                                                                        \
		  sdCircle(translate(point(), {var(CircleLine::POS_X), var(CircleLine::POS_Y)}), var(CircleLine::RADIUS)),     \
		  var(CircleLine::THICKNESS)))                                                                                 \
	X(BOX, Box, (POS_X, POS_Y, SIZE_X, SIZE_Y),                                                                        \
	  sdBox(translate(point(), {var(Box::POS_X), var(Box::POS_Y)}), {var(Box::SIZE_X), var(Box::SIZE_Y)}))             \
	X(BOX_LINE, BoxLine, (POS_X, POS_Y, SIZE_X, SIZE_Y, THICKNESS),                                                    \
	  sdfOnion(sdBox(translate(point(), {var(BoxLine::POS_X), var(BoxLine::POS_Y)}),                                   \
					 {var(BoxLine::SIZE_X), var(BoxLine::SIZE_Y)}),                                                    \
			   var(BoxLine::THICKNESS)))                                                                               \
	X(TRIANGLE, Triangle, (POS_X, POS_Y, WIDTH, HEIGHT),                                                               \
	  sdTriangle(translate(point(), {var(Triangle::POS_X), var(Triangle::POS_Y)}), var(Triangle::WIDTH),               \
				 var(Triangle::HEIGHT)))                                                                               \
	X(TRIANGLE_LINE, TriangleLine, (POS_X, POS_Y, WIDTH, HEIGHT, THICKNESS),                                           \
	  sdfOnion(sdTriangle(translate(point(), {var(TriangleLine::POS_X), var(TriangleLine::POS_Y)}),                    \
						  var(TriangleLine::WIDTH), var(TriangleLine::HEIGHT)),                                        \
			   var(TriangleLine::THICKNESS)))                                                                          \
	X(LINE, Line, (START_X, START_Y, END_X, END_Y, WIDTH),                                                             \
	  sdLine(point(), {var(Line::START_X), var(Line::START_Y)}, {var(Line::END_X), var(Line::END_Y)},                  \
			 var(Line::WIDTH)))                                                                                        \
	X(RAMP, Ramp, (POS_X, POS_Y, WIDTH, HEIGHT, SKEW),                                                                 \
	  sdRamp(translate(point(), {var(Ramp::POS_X), var(Ramp::POS_Y)}), var(Ramp::WIDTH), var(Ramp::HEIGHT),            \
			 var(Ramp::SKEW)))                                                                                         \
	X(SINE, SineWave, (AMPLITUDE, PERIOD, SPEED, OFFSET),                                                              \
	  sdSineWave(point(), var(SineWave::AMPLITUDE), var(SineWave::PERIOD), var(SineWave::SPEED),                       \
				 var(SineWave::OFFSET)))                                                                               \
	X(STAR, Star, (), getStarShape())                                                                                  \
	X(BOX_ROTATED, BoxRotated, (POS_X, POS_Y, SIZE_X, SIZE_Y, ANGLE),                                                  \
	  sdBox(rotate(translate(point(), {var(BoxRotated::POS_X), var(BoxRotated::POS_Y)}), var(BoxRotated::ANGLE)),      \
			{var(BoxRotated::SIZE_X), var(BoxRotated::SIZE_Y)}))                                                       \
	X(BOX_LINE_ROTATED, BoxLineRotated, (POS_X, POS_Y, SIZE_X, SIZE_Y, ANGLE, THICKNESS),                              \
	  sdfOnion(sdBox(rotate(translate(point(), {var(BoxLineRotated::POS_X), var(BoxLineRotated::POS_Y)}),              \
							var(BoxLineRotated::ANGLE)),                                                               \
					 {var(BoxLineRotated::SIZE_X), var(BoxLineRotated::SIZE_Y)}),                                      \
			   var(BoxLineRotated::THICKNESS)))                                                                        \
	X(TRIANGLE_ROTATED, TriangleRotated, (POS_X, POS_Y, WIDTH, HEIGHT, ANGLE),                                         \
	  sdTriangle(rotate(translate(point(), {var(TriangleRotated::POS_X), var(TriangleRotated::POS_Y)}),                \
						var(TriangleRotated::ANGLE)),                                                                  \
				 var(TriangleRotated::WIDTH), var(TriangleRotated::HEIGHT)))                                           \
	X(TRIANGLE_LINE_ROTATED, TriangleLineRotated, (POS_X, POS_Y, WIDTH, HEIGHT, ANGLE, THICKNESS),                     \
	  sdfOnion(                                                                                                        \
		  sdTriangle(rotate(translate(point(), {var(TriangleLineRotated::POS_X), var(TriangleLineRotated::POS_Y)}),    \
							var(TriangleLineRotated::ANGLE)),                                                          \
					 var(TriangleLineRotated::WIDTH), var(TriangleLineRotated::HEIGHT)),                               \
		  var(TriangleLineRotated::THICKNESS)))                                                                        \
	X(RAMP_ROTATED, RampRotated, (POS_X, POS_Y, WIDTH, HEIGHT, SKEW, ANGLE),                                           \
	  sdRamp(rotate(translate(point(), {var(RampRotated::POS_X), var(RampRotated::POS_Y)}), var(RampRotated::ANGLE)),  \
			 var(RampRotated::WIDTH), var(RampRotated::HEIGHT), var(RampRotated::SKEW)))

#define WEIRD_UNPACK_PARAMS_2D(...) __VA_ARGS__
#define WEIRD_GEN_PARAM_STRUCT_2D(ID, StructName, params, expr)                                                        \
	struct StructName                                                                                                  \
	{                                                                                                                  \
		enum Params : uint8_t                                                                                          \
		{                                                                                                              \
			WEIRD_UNPACK_PARAMS_2D params                                                                              \
		};                                                                                                             \
	};

		WEIRD_BUILTIN_SHAPES_2D(WEIRD_GEN_PARAM_STRUCT_2D)
#undef WEIRD_GEN_PARAM_STRUCT_2D
#undef WEIRD_UNPACK_PARAMS_2D

		enum : ShapeId
		{
#define WEIRD_SHAPE_2D_ENUM(ID, StructName, params, expr) ID,
			WEIRD_BUILTIN_SHAPES_2D(WEIRD_SHAPE_2D_ENUM)
#undef WEIRD_SHAPE_2D_ENUM
			COUNT_2D
		};

	} // namespace DefaultShapes
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT2DSDFS_H
