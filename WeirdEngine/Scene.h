#pragma once

#include "ECS.h"

#include <vector>
using namespace std;

#define _USE_MATH_DEFINES
#include <math.h>
#define radToDeg(angleInRadians) ((angleInRadians) * 180.0 / M_PI)

class Scene
{
private:
	
	const GLclampf RED, GREEN, BLUE, ALPHA;
	std::vector<Entity*> _entities;
	Entity* _camera;

public:

	Scene();

	Scene(const std::vector<Entity*>& entities) :  RED(1), GREEN(1), BLUE(1), ALPHA(1)
	{
		glClearColor(RED, GREEN, BLUE, ALPHA);
		_entities = entities;
	}


	
	void Update();
	void FixedUpdate();
	void Render();
	void AddBall();
};

