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
	std::vector<Entity*> m_entities;
	Entity* m_camera;

public:

	Scene();

	Scene(const std::vector<Entity*>& entities) :  RED(1), GREEN(1), BLUE(1), ALPHA(1)
	{
		glClearColor(RED, GREEN, BLUE, ALPHA);
		m_entities = entities;
	}

	~Scene() {
		for (auto it = begin(m_entities); it != end(m_entities); ++it) {
			delete* it;
		}
	}

	void Update();
	void FixedUpdate();
	void Render();
	void AddBall();
};

