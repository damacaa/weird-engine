#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "weird-engine/math/CompiledMathExpressions.h"
#include "weird-engine/math/MathExpressions.h"

namespace WeirdEngine::Primitives3D
{
    struct Plane : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_h;

    public:
        static constexpr uint8_t HEIGHT = 0;

        Plane(std::shared_ptr<IMathExpression> h) : m_h(std::move(h)) {}
        
        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override {
            return "fPlane(p, vec3(0.0, 1.0, 0.0), " + m_h->print() + ")\n";
        }
    };

    struct PerlinPlane : public WeirdEngine::IMathExpression
    {
    public:
        PerlinPlane(float height) : m_height(height) {}
        
        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override {
            return "fPlane(\n"
                   "    p, vec3(0.0, 1.0, 0.0), 3.0 + (0.5 * perlin(1.2 * vec2(p.x, p.z))) + (3.0 * perlin(0.2 * vec2(p.x, p.z))))\n";
        }
    private:
        float m_height;
    };

    struct Box : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_px, m_py, m_pz;
        std::shared_ptr<IMathExpression> m_sx, m_sy, m_sz;

    public:
        static constexpr uint8_t POS_X = 0;
        static constexpr uint8_t POS_Y = 1;
        static constexpr uint8_t POS_Z = 2;
        static constexpr uint8_t SIZE_X = 3;
        static constexpr uint8_t SIZE_Y = 4;
        static constexpr uint8_t SIZE_Z = 5;

        Box(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py, std::shared_ptr<IMathExpression> pz,
            std::shared_ptr<IMathExpression> sx, std::shared_ptr<IMathExpression> sy, std::shared_ptr<IMathExpression> sz)
            : m_px(std::move(px)), m_py(std::move(py)), m_pz(std::move(pz)),
              m_sx(std::move(sx)), m_sy(std::move(sy)), m_sz(std::move(sz))
        {
        }

        Box() // Legacy constructor for existing code compatibility
        {
        }

        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override
        {
            if (m_px)
            {
                return "fBox(p - vec3(" + m_px->print() + ", " + m_py->print() + ", " + m_pz->print() + "), vec3(" +
                       m_sx->print() + ", " + m_sy->print() + ", " + m_sz->print() + "))";
            }
            return "fBox(p - vec3(var0, var1, var2), vec3(var3, var4, var5))";
        }
    };

    struct Sphere : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_px, m_py, m_pz;
        std::shared_ptr<IMathExpression> m_r;

    public:
        static constexpr uint8_t POS_X = 0;
        static constexpr uint8_t POS_Y = 1;
        static constexpr uint8_t POS_Z = 2;
        static constexpr uint8_t RADIUS = 3;

        Sphere(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py, std::shared_ptr<IMathExpression> pz,
               std::shared_ptr<IMathExpression> r)
            : m_px(std::move(px)), m_py(std::move(py)), m_pz(std::move(pz)), m_r(std::move(r))
        {
        }

        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override
        {
            return "fSphere(p - vec3(" + m_px->print() + ", " + m_py->print() + ", " + m_pz->print() + "), " + m_r->print() + ")";
        }
    };

    struct Cylinder : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_px, m_py, m_pz;
        std::shared_ptr<IMathExpression> m_r, m_h;

    public:
        static constexpr uint8_t POS_X = 0;
        static constexpr uint8_t POS_Y = 1;
        static constexpr uint8_t POS_Z = 2;
        static constexpr uint8_t RADIUS = 3;
        static constexpr uint8_t HEIGHT = 4;

        Cylinder(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py, std::shared_ptr<IMathExpression> pz,
                 std::shared_ptr<IMathExpression> r, std::shared_ptr<IMathExpression> h)
            : m_px(std::move(px)), m_py(std::move(py)), m_pz(std::move(pz)), m_r(std::move(r)), m_h(std::move(h))
        {
        }

        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override
        {
            return "fCylinder(p - vec3(" + m_px->print() + ", " + m_py->print() + ", " + m_pz->print() + "), " + m_r->print() + ", " + m_h->print() + ")";
        }
    };

    struct Torus : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_px, m_py, m_pz;
        std::shared_ptr<IMathExpression> m_r1, m_r2;

    public:
        static constexpr uint8_t POS_X = 0;
        static constexpr uint8_t POS_Y = 1;
        static constexpr uint8_t POS_Z = 2;
        static constexpr uint8_t RADIUS_SMALL = 3;
        static constexpr uint8_t RADIUS_LARGE = 4;

        Torus(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py, std::shared_ptr<IMathExpression> pz,
              std::shared_ptr<IMathExpression> r1, std::shared_ptr<IMathExpression> r2)
            : m_px(std::move(px)), m_py(std::move(py)), m_pz(std::move(pz)), m_r1(std::move(r1)), m_r2(std::move(r2))
        {
        }

        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override
        {
            return "fTorus(p - vec3(" + m_px->print() + ", " + m_py->print() + ", " + m_pz->print() + "), " + m_r1->print() + ", " + m_r2->print() + ")";
        }
    };

    struct Capsule : public WeirdEngine::IMathExpression
    {
    protected:
        std::shared_ptr<IMathExpression> m_px, m_py, m_pz;
        std::shared_ptr<IMathExpression> m_r, m_h;

    public:
        static constexpr uint8_t POS_X = 0;
        static constexpr uint8_t POS_Y = 1;
        static constexpr uint8_t POS_Z = 2;
        static constexpr uint8_t RADIUS = 3;
        static constexpr uint8_t HEIGHT = 4;

        Capsule(std::shared_ptr<IMathExpression> px, std::shared_ptr<IMathExpression> py, std::shared_ptr<IMathExpression> pz,
                std::shared_ptr<IMathExpression> r, std::shared_ptr<IMathExpression> h)
            : m_px(std::move(px)), m_py(std::move(py)), m_pz(std::move(pz)), m_r(std::move(r)), m_h(std::move(h))
        {
        }

        float getValue(const float* parameters) const override { return 1000.0f; }
        std::string print() const override
        {
            return "fCapsule(p - vec3(" + m_px->print() + ", " + m_py->print() + ", " + m_pz->print() + "), " + m_r->print() + ", " + m_h->print() + ")";
        }
    };
} // namespace WeirdEngine::Primitives3D
