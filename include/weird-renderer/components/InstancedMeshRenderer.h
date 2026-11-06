#pragma once
#include "weird-renderer/resources/Mesh.h"

namespace WeirdEngine
{
	struct InstancedMeshRenderer
	{
		WeirdRenderer::MeshID meshID = 0;
		glm::vec3 translation = glm::vec3(0.0f);
		glm::vec3 scale = glm::vec3(1.0f);
		glm::vec3 rotation = glm::vec3(0.0f);
	};
} // namespace WeirdEngine
