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
	X(Circle, (PosX, PosY, Radius),                                                                                    \
	  sdCircle(translate(point(), {var(Circle::PosX), var(Circle::PosY)}), var(Circle::Radius)))                       \
	X(CircleLine, (PosX, PosY, Radius, Thickness),                                                                     \
	  sdfOnion(sdCircle(translate(point(), {var(CircleLine::PosX), var(CircleLine::PosY)}), var(CircleLine::Radius)),  \
			   var(CircleLine::Thickness)))                                                                            \
	X(Box, (PosX, PosY, SizeX, SizeY),                                                                                 \
	  sdBox(translate(point(), {var(Box::PosX), var(Box::PosY)}), {var(Box::SizeX), var(Box::SizeY)}))                 \
	X(BoxLine, (PosX, PosY, SizeX, SizeY, Thickness),                                                                  \
	  sdfOnion(sdBox(translate(point(), {var(BoxLine::PosX), var(BoxLine::PosY)}),                                     \
					 {var(BoxLine::SizeX), var(BoxLine::SizeY)}),                                                      \
			   var(BoxLine::Thickness)))                                                                               \
	X(Triangle, (PosX, PosY, Width, Height),                                                                           \
	  sdTriangle(translate(point(), {var(Triangle::PosX), var(Triangle::PosY)}), var(Triangle::Width),                 \
				 var(Triangle::Height)))                                                                               \
	X(TriangleLine, (PosX, PosY, Width, Height, Thickness),                                                            \
	  sdfOnion(sdTriangle(translate(point(), {var(TriangleLine::PosX), var(TriangleLine::PosY)}),                      \
						  var(TriangleLine::Width), var(TriangleLine::Height)),                                        \
			   var(TriangleLine::Thickness)))                                                                          \
	X(Line, (StartX, StartY, EndX, EndY, Width),                                                                       \
	  sdLine(point(), {var(Line::StartX), var(Line::StartY)}, {var(Line::EndX), var(Line::EndY)}, var(Line::Width)))   \
	X(Ramp, (PosX, PosY, Width, Height, Skew),                                                                         \
	  sdRamp(translate(point(), {var(Ramp::PosX), var(Ramp::PosY)}), var(Ramp::Width), var(Ramp::Height),              \
			 var(Ramp::Skew)))                                                                                         \
	X(SineWave, (Amplitude, Period, Speed, Offset),                                                                    \
	  sdSineWave(point(), var(SineWave::Amplitude), var(SineWave::Period), var(SineWave::Speed),                       \
				 var(SineWave::Offset)))                                                                               \
	X(Star, (), getStarShape())                                                                                        \
	X(BoxRotated, (PosX, PosY, SizeX, SizeY, Angle),                                                                   \
	  sdBox(rotate(translate(point(), {var(BoxRotated::PosX), var(BoxRotated::PosY)}), var(BoxRotated::Angle)),        \
			{var(BoxRotated::SizeX), var(BoxRotated::SizeY)}))                                                         \
	X(BoxLineRotated, (PosX, PosY, SizeX, SizeY, Angle, Thickness),                                                    \
	  sdfOnion(sdBox(rotate(translate(point(), {var(BoxLineRotated::PosX), var(BoxLineRotated::PosY)}),                \
							var(BoxLineRotated::Angle)),                                                               \
					 {var(BoxLineRotated::SizeX), var(BoxLineRotated::SizeY)}),                                        \
			   var(BoxLineRotated::Thickness)))                                                                        \
	X(TriangleRotated, (PosX, PosY, Width, Height, Angle),                                                             \
	  sdTriangle(rotate(translate(point(), {var(TriangleRotated::PosX), var(TriangleRotated::PosY)}),                  \
						var(TriangleRotated::Angle)),                                                                  \
				 var(TriangleRotated::Width), var(TriangleRotated::Height)))                                           \
	X(TriangleLineRotated, (PosX, PosY, Width, Height, Angle, Thickness),                                              \
	  sdfOnion(sdTriangle(rotate(translate(point(), {var(TriangleLineRotated::PosX), var(TriangleLineRotated::PosY)}), \
								 var(TriangleLineRotated::Angle)),                                                     \
						  var(TriangleLineRotated::Width), var(TriangleLineRotated::Height)),                          \
			   var(TriangleLineRotated::Thickness)))                                                                   \
	X(RampRotated, (PosX, PosY, Width, Height, Skew, Angle),                                                           \
	  sdRamp(rotate(translate(point(), {var(RampRotated::PosX), var(RampRotated::PosY)}), var(RampRotated::Angle)),    \
			 var(RampRotated::Width), var(RampRotated::Height), var(RampRotated::Skew)))

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
