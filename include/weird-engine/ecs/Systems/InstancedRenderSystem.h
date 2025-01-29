#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"
#define min(a, b) a < b ? a : b



class InstancedRenderSystem : public System {
private:
	const size_t MAX_INSTANCES = 255;
	std::shared_ptr<ComponentManager> m_iRendererManager;

public:

	InstancedRenderSystem(ECSManager& ecs)
	{
		m_iRendererManager = ecs.getComponentManager<InstancedMeshRenderer>();
	}

	void render(ECSManager& ecs, ResourceManager& resourceManager, WeirdRenderer::Shader& shader, WeirdRenderer::Camera& camera, const std::vector<WeirdRenderer::Light>& lights) {

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

	}
};