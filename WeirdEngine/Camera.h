#pragma once
#include <GL/glut.h>
#include "Component.h"

class Camera : public Component
{
private:

public:
	Camera(Entity* owner):Component(owner) {};
	void Render();
};



