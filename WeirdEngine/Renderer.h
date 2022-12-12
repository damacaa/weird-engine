#pragma once

#include <GL/glut.h>
#include "Component.h"
#include "Color.h"

class Renderer : public Component
{
public:
	Color _color;

	Renderer(Entity* owner) :Component(owner), _color(Color(1, 1, 1)) {};

	void Render() = 0;
};

