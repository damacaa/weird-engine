#pragma once
#include "../ECS.h"

class InstancedRenderSystem : public System {

public:
	InstancedRenderSystem() {

		m_entities = std::vector<Entity>();

	}


	void render(ECS& ecs, Shader& shader, Camera& camera, const std::vector<Light>& lights) {


		auto& componentArray = GetManagerArray<InstancedMeshRenderer>();

		if (componentArray.getSize() == 0)
			return;

		auto& firstMesh = componentArray[0];

		std::vector<glm::vec3> translations;
		std::vector<glm::vec3> rotations;
		std::vector<glm::vec3> scales;

		for (size_t i = 0; i < componentArray.getSize(); i++)
		{
			auto& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			translations.push_back(t.position);
			rotations.push_back(glm::vec3(0));
			scales.push_back(glm::vec3(1));

			//mr.mesh.Draw(shader, camera, glm::vec3(t.x, t.y, t.z), glm::vec3(0), glm::vec3(1), lights);
		}

		firstMesh.mesh.DrawInstance(shader, camera, componentArray.getSize(), translations, rotations, scales, lights);
	}

};