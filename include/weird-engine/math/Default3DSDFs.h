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
	X(Plane, (Height), std::make_shared<Primitives3D::Plane>(DefaultShapes3D::var(Plane::Height)))                     \
	X(Box, (PosX, PosY, PosZ, SizeX, SizeY, SizeZ),                                                                    \
	  std::make_shared<Primitives3D::Box>(DefaultShapes3D::var(Box::PosX), DefaultShapes3D::var(Box::PosY),            \
										  DefaultShapes3D::var(Box::PosZ), DefaultShapes3D::var(Box::SizeX),           \
										  DefaultShapes3D::var(Box::SizeY), DefaultShapes3D::var(Box::SizeZ)))         \
	X(Sphere, (PosX, PosY, PosZ, Radius),                                                                              \
	  std::make_shared<Primitives3D::Sphere>(DefaultShapes3D::var(Sphere::PosX), DefaultShapes3D::var(Sphere::PosY),   \
											 DefaultShapes3D::var(Sphere::PosZ),                                       \
											 DefaultShapes3D::var(Sphere::Radius)))                                    \
	X(Cylinder, (PosX, PosY, PosZ, Radius, Height),                                                                    \
	  std::make_shared<Primitives3D::Cylinder>(                                                                        \
		  DefaultShapes3D::var(Cylinder::PosX), DefaultShapes3D::var(Cylinder::PosY),                                  \
		  DefaultShapes3D::var(Cylinder::PosZ), DefaultShapes3D::var(Cylinder::Radius),                                \
		  DefaultShapes3D::var(Cylinder::Height)))                                                                     \
	X(Torus, (PosX, PosY, PosZ, RadiusSmall, RadiusLarge),                                                             \
	  std::make_shared<Primitives3D::Torus>(                                                                           \
		  DefaultShapes3D::var(Torus::PosX), DefaultShapes3D::var(Torus::PosY), DefaultShapes3D::var(Torus::PosZ),     \
		  DefaultShapes3D::var(Torus::RadiusSmall), DefaultShapes3D::var(Torus::RadiusLarge)))                         \
	X(Capsule, (PosX, PosY, PosZ, Radius, Height),                                                                     \
	  std::make_shared<Primitives3D::Capsule>(                                                                         \
		  DefaultShapes3D::var(Capsule::PosX), DefaultShapes3D::var(Capsule::PosY),                                    \
		  DefaultShapes3D::var(Capsule::PosZ), DefaultShapes3D::var(Capsule::Radius),                                  \
		  DefaultShapes3D::var(Capsule::Height)))

namespace WeirdEngine
{
	namespace DefaultShapes3D
	{
		inline auto var(uint8_t index)
		{
			return std::make_shared<detail::FloatVariable>(index);
		}

#define WEIRD_UNPACK_PARAMS_3D(...) __VA_ARGS__
#define WEIRD_GEN_PARAM_STRUCT_3D(Name, params, expr)                                                                  \
	struct Name                                                                                                        \
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
#define WEIRD_SHAPE_3D_ENUM(Name, params, expr) Name,
			WEIRD_BUILTIN_SHAPES_3D(WEIRD_SHAPE_3D_ENUM)
#undef WEIRD_SHAPE_3D_ENUM
			COUNT_3D
		};

		constexpr size_t TOTAL_BUILTIN_SHAPES = COUNT_3D;
	} // namespace DefaultShapes3D
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT3DSDFS_H
