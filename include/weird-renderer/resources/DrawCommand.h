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
			// Material* material; // TODO
			glm::mat4 transform;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine

#endif // DRAWCOMMAND_H
