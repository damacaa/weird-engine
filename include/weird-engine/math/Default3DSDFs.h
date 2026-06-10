#ifndef WEIRDSAMPLES_DEFAULT3DSDFS_H
#define WEIRDSAMPLES_DEFAULT3DSDFS_H

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "weird-engine/math/CompiledMathExpressions.h"
#include "weird-engine/math/MathExpressions.h"
#include "weird-engine/math/Primitives3D.h"

#include "weird-engine/Scene.h"

namespace WeirdEngine
{
    namespace DefaultShapes3D
    {
        inline auto var(uint8_t index) { return std::make_shared<FloatVariable>(index); }

        inline const uint16_t BOX = Scene::registerDefaultSDF(std::make_shared<Primitives3D::Box>(
            var(Primitives3D::Box::POS_X), var(Primitives3D::Box::POS_Y), var(Primitives3D::Box::POS_Z),
            var(Primitives3D::Box::SIZE_X), var(Primitives3D::Box::SIZE_Y), var(Primitives3D::Box::SIZE_Z)
        ));

        inline const uint16_t SPHERE = Scene::registerDefaultSDF(std::make_shared<Primitives3D::Sphere>(
            var(Primitives3D::Sphere::POS_X), var(Primitives3D::Sphere::POS_Y), var(Primitives3D::Sphere::POS_Z),
            var(Primitives3D::Sphere::RADIUS)
        ));

        inline const uint16_t CYLINDER = Scene::registerDefaultSDF(std::make_shared<Primitives3D::Cylinder>(
            var(Primitives3D::Cylinder::POS_X), var(Primitives3D::Cylinder::POS_Y), var(Primitives3D::Cylinder::POS_Z),
            var(Primitives3D::Cylinder::RADIUS), var(Primitives3D::Cylinder::HEIGHT)
        ));

        inline const uint16_t TORUS = Scene::registerDefaultSDF(std::make_shared<Primitives3D::Torus>(
            var(Primitives3D::Torus::POS_X), var(Primitives3D::Torus::POS_Y), var(Primitives3D::Torus::POS_Z),
            var(Primitives3D::Torus::RADIUS_SMALL), var(Primitives3D::Torus::RADIUS_LARGE)
        ));

        inline const uint16_t CAPSULE = Scene::registerDefaultSDF(std::make_shared<Primitives3D::Capsule>(
            var(Primitives3D::Capsule::POS_X), var(Primitives3D::Capsule::POS_Y), var(Primitives3D::Capsule::POS_Z),
            var(Primitives3D::Capsule::RADIUS), var(Primitives3D::Capsule::HEIGHT)
        ));

    } // namespace DefaultShapes3D
} // namespace WeirdEngine

#endif // WEIRDSAMPLES_DEFAULT3DSDFS_H
