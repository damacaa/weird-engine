#pragma once
#pragma once
#include "weird-engine/ecs/Component.h"
#include"weird-renderer/resources/Mesh.h"


namespace WeirdEngine
{
	struct InstancedMeshRenderer : public Component {
	private:

	public:

		InstancedMeshRenderer() {};

		InstancedMeshRenderer(WeirdRenderer::MeshID mesh) : meshID(mesh) {	};

		WeirdRenderer::MeshID meshID;
		glm::vec3 translation;
		glm::vec3 scale;
		glm::vec3 rotation;

	};
}

