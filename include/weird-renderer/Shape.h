#include<glm/glm.hpp>

#ifndef SHAPE_CLASS_H
#define SHAPE_CLASS_H

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct Shape {
			glm::vec3 position;
			float size = 0.5f;
		};
	}
}

#endif

