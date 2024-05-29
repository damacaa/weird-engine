#pragma once
#pragma once
#include "../ECS.h"
#include"../../../weird-renderer/Mesh.h"


struct InstancedMeshRenderer : public Component {
private:

public:

	InstancedMeshRenderer() {};

	InstancedMeshRenderer(Mesh mesh) : mesh(mesh) {


	};

	Mesh mesh;
	glm::vec3 translation;
	glm::vec3 scale;
	glm::vec3 rotation;

};


class InstancedRenderSystem : public System {

public:
	InstancedRenderSystem() {

		entities = std::vector<Entity>();

	}


	void render(ECS& ecs, Shader& shader, Camera& camera, const std::vector<Light>& lights) {


		std::shared_ptr<ComponentArray< InstancedMeshRenderer>> arrayy = manager->getComponentArray<InstancedMeshRenderer>();

		const int size = arrayy->size;
		auto& firstMesh = arrayy->componentArray[0];

		std::vector<glm::vec3> translations;
		std::vector<glm::vec3> rotations;
		std::vector<glm::vec3> scales;

		for (size_t i = 0; i < size; i++)
		{

			auto& mr = arrayy->componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			translations.push_back(glm::vec3(t.x, t.y, t.z));
			rotations.push_back(glm::vec3(0));
			scales.push_back(glm::vec3(1));

			//mr.mesh.Draw(shader, camera, glm::vec3(t.x, t.y, t.z), glm::vec3(0), glm::vec3(1), lights);
		}

		firstMesh.mesh.DrawInstance(shader, camera, size, translations, rotations, scales, lights);
	}

};