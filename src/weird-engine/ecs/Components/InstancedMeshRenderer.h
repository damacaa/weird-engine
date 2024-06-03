#pragma once
#pragma once
#include "../Component.h"
#include"../../../weird-renderer/Mesh.h"


struct InstancedMeshRenderer : public Component {
private:

public:

	InstancedMeshRenderer() {};

	InstancedMeshRenderer(Mesh mesh) : mesh(mesh) {	};

	InstancedMeshRenderer(MeshID mesh) : meshID(mesh) {	};

	Mesh mesh;
	MeshID meshID;
	glm::vec3 translation;
	glm::vec3 scale;
	glm::vec3 rotation;

};


