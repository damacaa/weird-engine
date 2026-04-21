#pragma once
#include <weird-engine.h>
#include <weird-engine/math/MathExpressions.h>


struct Plane : public WeirdEngine::IMathExpression
{
public:
	Plane(float height)
		: m_height(height)
	{
	}

	float getValue() const override
	{
		return 1000.0f;
	}

  void propagateValues(float* values) override
  {
      // No variables to propagate since this plane is defined by a constant height
  }

	std::string print() const override
	{
    std::string code = 
        "fPlane(p, vec3(0.0, 1.0, 0.0), 3.0)\n";

		return code;
	}

private:
	float m_height;
};

struct PerlinPlane : public WeirdEngine::IMathExpression
{
public:
	PerlinPlane(float height)
		: m_height(height)
	{
	}

	float getValue() const override
	{
		return 1000.0f;
	}

  void propagateValues(float* values) override
  {
      // No variables to propagate since this plane is defined by a constant height
  }

	std::string print() const override
	{
    std::string code = 
        "fPlane(\n"
        "    p, vec3(0.0, 1.0, 0.0), 3.0 + (0.5 * perlin(1.2 * vec2(p.x, p.z))) + (3.0 * perlin(0.2 * vec2(p.x, p.z))))\n";

		return code;
	}

private:
	float m_height;
};