#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"
#define min(a, b) a < b ? a : b

class InstancedRenderSystem : public System {
private:
	const size_t MAX_INSTANCES = 255;
	std::shared_ptr<ComponentManager> m_iRendererManager;

public:

	InstancedRenderSystem(ECS& ecs) 
	{
		m_iRendererManager = ecs.getComponentManager<InstancedMeshRenderer>();
	}

	void render(ECS& ecs, ResourceManager& resourceManager, Shader& shader, Camera& camera, const std::vector<Light>& lights) {

		shader.activate();
		shader.setUniform("lightColor", lights[0].color);
		shader.setUniform("lightPos", lights[0].position);

		auto& componentArray = *m_iRendererManager->getComponentArray<InstancedMeshRenderer>();

		if (componentArray.getSize() == 0)
			return;

		auto& meshRenderer = componentArray[0];


		int arraySize = componentArray.getSize();


		std::unordered_map<MeshID, std::vector<Transform>> transformMap;

		for (size_t i = 0; i < arraySize; i++)
		{

			auto& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			auto id = mr.meshID;

			auto& vector = transformMap[id];
			vector.push_back(t);

			if (vector.size() == MAX_INSTANCES) {
				resourceManager.getMesh(mr.meshID).DrawInstance(shader, camera, MAX_INSTANCES, vector, lights);
				vector.clear();
			}
		}

		for (auto& pair : transformMap)
		{
			resourceManager.getMesh(pair.first).DrawInstance(shader, camera, pair.second.size(), pair.second, lights);
		}

		return;
		int iterations = (arraySize / MAX_INSTANCES) + 1;

		if (iterations > 1)
			int stop = 1;

		int k = 0;
		for (size_t i = 0; i < iterations; i++)
		{

			int count = i == iterations - 1
				?
				arraySize - (i * MAX_INSTANCES)
				:
				MAX_INSTANCES;

			if (count == 0)
				break;


			/*std::vector<glm::vec3> translations;
			std::vector<glm::vec3> rotations;
			std::vector<glm::vec3> scales;

			// 5fps drop
			for (size_t j = 0; j < count; j++)
			{
				auto& mr = componentArray[k];
				auto& t = ecs.getComponent<Transform>(mr.Owner);

				translations.push_back(t.position);
				rotations.push_back(t.rotation);
				scales.push_back(t.scale);

				k++;
			}

			mesh.mesh.DrawInstance(shader, camera, count, translations, rotations, scales, lights);*/

			std::vector<Transform> transforms;
			for (size_t j = 0; j < count; j++)
			{
				auto& mr = componentArray[k];
				auto& t = ecs.getComponent<Transform>(mr.Owner);
				transforms.push_back(t);

				k++;
			}

			resourceManager.getMesh(meshRenderer.meshID).DrawInstance(shader, camera, count, transforms, lights);
		}
	}

};