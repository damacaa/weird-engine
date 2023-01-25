#pragma once
#include "Renderer.h"

class PrimitiveRenderer : public Renderer
{
public:
	enum class Primitive { Sphere, BigSphere, Cube, Cone, Torus, Dodecahedron, Octahedron, Tetrahedron, Icosahedron, Teapot };

private:

public:
	Primitive m_primitive;

	PrimitiveRenderer():Renderer(), m_primitive(Primitive::Sphere) {};

	Primitive GetPrimitive() { return m_primitive; }
	void Render() override;
};

