#include<glm/glm.hpp>

#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H
struct Shape {
	glm::vec3 position;
	float a = 0;
	glm::vec3 size{ 0.5 };
	float b = 0;
};

#endif

