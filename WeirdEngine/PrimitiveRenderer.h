#pragma once
#include "Renderer.h"

class PrimitiveRenderer : public Renderer
{
public:
	enum class Primitive { Sphere, Cube, Cone, Torus, Dodecahedron, Octahedron, Tetrahedron, Icosahedron, Teapot };

private:

public:
	Primitive _primitive;

	PrimitiveRenderer(Entity* owner) :Renderer(owner), _primitive(Primitive::Sphere) {};
	void Render();

};

