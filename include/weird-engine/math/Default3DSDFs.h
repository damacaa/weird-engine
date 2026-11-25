#ifndef WEIRDSAMPLES_DEFAULT3DSDFS_H
#define WEIRDSAMPLES_DEFAULT3DSDFS_H

#include <cstdint>
#include <string_view>

#include "SDF.h"

#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/Scene.h"

namespace WeirdEngine
{
	namespace DefaultShapes3D
	{
		using namespace SDF;

#define WEIRD_BUILTIN_SHAPES_3D(X)                                                                                     \
	X(Plane, (Height), sdPlane(point3D(), var(Plane::Height)))                                                         \
	X(Box, (PosX, PosY, PosZ, SizeX, SizeY, SizeZ),                                                                    \
	  sdBox(translate(point3D(), {var(Box::PosX), var(Box::PosY), var(Box::PosZ)}),                                    \
			{var(Box::SizeX), var(Box::SizeY), var(Box::SizeZ)}))                                                      \
	X(Sphere, (PosX, PosY, PosZ, Radius),                                                                              \
	  sdSphere(translate(point3D(), {var(Sphere::PosX), var(Sphere::PosY), var(Sphere::PosZ)}), var(Sphere::Radius)))  \
	X(Cylinder, (PosX, PosY, PosZ, Radius, Height),                                                                    \
	  sdCylinder(translate(point3D(), {var(Cylinder::PosX), var(Cylinder::PosY), var(Cylinder::PosZ)}),                \
				 var(Cylinder::Radius), var(Cylinder::Height)))                                                        \
	X(Torus, (PosX, PosY, PosZ, MajorRadius, MinorRadius),                                                             \
	  sdTorus(translate(point3D(), {var(Torus::PosX), var(Torus::PosY), var(Torus::PosZ)}), var(Torus::MajorRadius),   \
			  var(Torus::MinorRadius)))                                                                                \
	X(Capsule, (PosX, PosY, PosZ, Radius, Height),                                                                     \
	  sdCapsule(translate(point3D(), {var(Capsule::PosX), var(Capsule::PosY), var(Capsule::PosZ)}),                    \
				var(Capsule::Radius), var(Capsule::Height)))

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

	namespace BuiltinShapes
	{
		constexpr const char* toName(ShapeId id) noexcept
		{
			switch (id)
			{
#define WEIRD_BUILTIN_2D_TO_NAME(Name, params, expr)                                                                   \
	case DefaultShapes::Name:                                                                                          \
		return #Name;
				WEIRD_BUILTIN_SHAPES_2D(WEIRD_BUILTIN_2D_TO_NAME)
#undef WEIRD_BUILTIN_2D_TO_NAME
#define WEIRD_BUILTIN_3D_TO_NAME(Name, params, expr)                                                                   \
	case DefaultShapes3D::Name:                                                                                        \
		return "3D/" #Name;
				WEIRD_BUILTIN_SHAPES_3D(WEIRD_BUILTIN_3D_TO_NAME)
#undef WEIRD_BUILTIN_3D_TO_NAME
				default:
					return nullptr;
			}
		}

		constexpr bool fromName(std::string_view name, ShapeId& outId) noexcept
		{
#define WEIRD_BUILTIN_2D_FROM_NAME(Name, params, expr)                                                                 \
	if (name == #Name)                                                                                                 \
	{                                                                                                                  \
		outId = DefaultShapes::Name;                                                                                   \
		return true;                                                                                                   \
	}
			WEIRD_BUILTIN_SHAPES_2D(WEIRD_BUILTIN_2D_FROM_NAME)
#undef WEIRD_BUILTIN_2D_FROM_NAME
#define WEIRD_BUILTIN_3D_FROM_NAME(Name, params, expr)                                                                 \
	if (name == "3D/" #Name)                                                                                           \
	{                                                                                                                  \
		outId = DefaultShapes3D::Name;                                                                                 \
		return true;                                                                                                   \
	}
			WEIRD_BUILTIN_SHAPES_3D(WEIRD_BUILTIN_3D_FROM_NAME)
#undef WEIRD_BUILTIN_3D_FROM_NAME
			return false;
		}
	} // namespace BuiltinShapes
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT3DSDFS_H
