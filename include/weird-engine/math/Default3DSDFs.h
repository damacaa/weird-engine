#ifndef WEIRDSAMPLES_DEFAULT3DSDFS_H
#define WEIRDSAMPLES_DEFAULT3DSDFS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/math/Primitives3D.h"

#include "weird-engine/Scene.h"

#include "weird-engine/math/Default2DSDFs.h"

#define WEIRD_BUILTIN_SHAPES_3D(X)                                                                                     \
	X(PLANE, Plane, (HEIGHT), std::make_shared<Primitives3D::Plane>(DefaultShapes3D::var(Plane::HEIGHT)))              \
	X(BOX, Box, (POS_X, POS_Y, POS_Z, SIZE_X, SIZE_Y, SIZE_Z),                                                         \
	  std::make_shared<Primitives3D::Box>(DefaultShapes3D::var(Box::POS_X), DefaultShapes3D::var(Box::POS_Y),          \
										  DefaultShapes3D::var(Box::POS_Z), DefaultShapes3D::var(Box::SIZE_X),         \
										  DefaultShapes3D::var(Box::SIZE_Y), DefaultShapes3D::var(Box::SIZE_Z)))       \
	X(SPHERE, Sphere, (POS_X, POS_Y, POS_Z, RADIUS),                                                                   \
	  std::make_shared<Primitives3D::Sphere>(DefaultShapes3D::var(Sphere::POS_X), DefaultShapes3D::var(Sphere::POS_Y), \
											 DefaultShapes3D::var(Sphere::POS_Z),                                      \
											 DefaultShapes3D::var(Sphere::RADIUS)))                                    \
	X(CYLINDER, Cylinder, (POS_X, POS_Y, POS_Z, RADIUS, HEIGHT),                                                       \
	  std::make_shared<Primitives3D::Cylinder>(                                                                        \
		  DefaultShapes3D::var(Cylinder::POS_X), DefaultShapes3D::var(Cylinder::POS_Y),                                \
		  DefaultShapes3D::var(Cylinder::POS_Z), DefaultShapes3D::var(Cylinder::RADIUS),                               \
		  DefaultShapes3D::var(Cylinder::HEIGHT)))                                                                     \
	X(TORUS, Torus, (POS_X, POS_Y, POS_Z, RADIUS_SMALL, RADIUS_LARGE),                                                 \
	  std::make_shared<Primitives3D::Torus>(                                                                           \
		  DefaultShapes3D::var(Torus::POS_X), DefaultShapes3D::var(Torus::POS_Y), DefaultShapes3D::var(Torus::POS_Z),  \
		  DefaultShapes3D::var(Torus::RADIUS_SMALL), DefaultShapes3D::var(Torus::RADIUS_LARGE)))                       \
	X(CAPSULE, Capsule, (POS_X, POS_Y, POS_Z, RADIUS, HEIGHT),                                                         \
	  std::make_shared<Primitives3D::Capsule>(                                                                         \
		  DefaultShapes3D::var(Capsule::POS_X), DefaultShapes3D::var(Capsule::POS_Y),                                  \
		  DefaultShapes3D::var(Capsule::POS_Z), DefaultShapes3D::var(Capsule::RADIUS),                                 \
		  DefaultShapes3D::var(Capsule::HEIGHT)))

namespace WeirdEngine
{
	namespace DefaultShapes3D
	{
		inline auto var(uint8_t index)
		{
			return std::make_shared<detail::FloatVariable>(index);
		}

#define WEIRD_UNPACK_PARAMS_3D(...) __VA_ARGS__
#define WEIRD_GEN_PARAM_STRUCT_3D(ID, StructName, params, expr)                                                        \
	struct StructName                                                                                                  \
	{                                                                                                                  \
		enum Params : uint8_t                                                                                          \
		{                                                                                                              \
			WEIRD_UNPACK_PARAMS_3D params                                                                              \
		};                                                                                                             \
	};
		WEIRD_BUILTIN_SHAPES_3D(WEIRD_GEN_PARAM_STRUCT_3D)
#undef WEIRD_GEN_PARAM_STRUCT_3D
#undef WEIRD_UNPACK_PARAMS_3D

		enum : ShapeId
		{
			_OFFSET_3D = DefaultShapes::COUNT_2D - 1,
#define WEIRD_SHAPE_3D_ENUM(ID, StructName, params, expr) ID,
			WEIRD_BUILTIN_SHAPES_3D(WEIRD_SHAPE_3D_ENUM)
#undef WEIRD_SHAPE_3D_ENUM
			COUNT_3D
		};

		constexpr size_t TOTAL_BUILTIN_SHAPES = COUNT_3D;
	} // namespace DefaultShapes3D
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT3DSDFS_H
