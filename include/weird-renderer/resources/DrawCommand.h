#ifndef DRAWCOMMAND_H
#define DRAWCOMMAND_H

#include "weird-renderer/resources/Mesh.h"
// #include "Material.h" // BIG TODO

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		// The data structure
		struct DrawCommand
		{
			Mesh* mesh;
			int materialIndex = 0;
			glm::vec3 translation;
			glm::vec3 rotation;
			glm::vec3 scale;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif // DRAWCOMMAND_H
