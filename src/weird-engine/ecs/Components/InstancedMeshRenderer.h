#pragma once
#pragma once
#include "../Component.h"
#include"../../../weird-renderer/Mesh.h"



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


