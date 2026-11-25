#ifndef WEIRDSAMPLES_DEFAULT2DSDFS_H
#define WEIRDSAMPLES_DEFAULT2DSDFS_H

#include <cstdint>

#include "SDF.h"
#include "StarShape.h"

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace DefaultShapes
	{
		using namespace SDF;

#define WEIRD_BUILTIN_SHAPES_2D(X)                                                                                     \
	X(Circle, (PosX, PosY, Radius, OutlineThickness),                                                                  \
	  sdfOutline(sdCircle(translate(point(), {var(Circle::PosX), var(Circle::PosY)}), var(Circle::Radius)),            \
				 var(Circle::OutlineThickness)))                                                                       \
	X(Box, (PosX, PosY, SizeX, SizeY, Radius, Angle, OutlineThickness),                                                \
	  sdfOutline(sdRoundBox(rotate(translate(point(), {var(Box::PosX), var(Box::PosY)}), var(Box::Angle)),             \
							{var(Box::SizeX), var(Box::SizeY)}, var(Box::Radius)),                                     \
				 var(Box::OutlineThickness)))                                                                          \
	X(Triangle, (PosX, PosY, Width, Height, Radius, Angle, OutlineThickness),                                          \
	  sdfOutline(sdRoundTriangle(                                                                                      \
					 rotate(translate(point(), {var(Triangle::PosX), var(Triangle::PosY)}), var(Triangle::Angle)),     \
					 var(Triangle::Width), var(Triangle::Height), var(Triangle::Radius)),                              \
				 var(Triangle::OutlineThickness)))                                                                     \
	X(Line, (StartX, StartY, EndX, EndY, Width, OutlineThickness),                                                     \
	  sdfOutline(sdLine(point(), {var(Line::StartX), var(Line::StartY)}, {var(Line::EndX), var(Line::EndY)},           \
						var(Line::Width)),                                                                             \
				 var(Line::OutlineThickness)))                                                                         \
	X(Ramp, (PosX, PosY, Width, Height, Skew, Angle, OutlineThickness),                                                \
	  sdfOutline(sdRamp(rotate(translate(point(), {var(Ramp::PosX), var(Ramp::PosY)}), var(Ramp::Angle)),              \
						var(Ramp::Width), var(Ramp::Height), var(Ramp::Skew)),                                         \
				 var(Ramp::OutlineThickness)))                                                                         \
	X(SineWave, (Amplitude, Period, Speed, Offset, OutlineThickness),                                                  \
	  sdfOutline(sdSineWave(point(), var(SineWave::Amplitude), var(SineWave::Period), var(SineWave::Speed),            \
							var(SineWave::Offset)),                                                                    \
				 var(SineWave::OutlineThickness)))                                                                     \
	X(Star, (PosX, PosY, Radius, Displacement, Points, Speed, OutlineThickness),                                       \
	  sdfOutline(sdStar(translate(point(), {var(Star::PosX), var(Star::PosY)}), var(Star::Radius),                     \
						var(Star::Displacement), var(Star::Points), var(Star::Speed)),                                 \
				 var(Star::OutlineThickness)))

#define WEIRD_UNPACK_PARAMS_2D(...) __VA_ARGS__
#define WEIRD_GEN_PARAM_STRUCT_2D(Name, params, expr)                                                                  \
	struct Name                                                                                                        \
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
#define WEIRD_SHAPE_2D_ENUM(Name, params, expr) Name,
			WEIRD_BUILTIN_SHAPES_2D(WEIRD_SHAPE_2D_ENUM)
#undef WEIRD_SHAPE_2D_ENUM
			COUNT_2D
		};

	} // namespace DefaultShapes
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT2DSDFS_H
