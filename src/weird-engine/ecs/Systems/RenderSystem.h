#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"

class RenderSystem : public System {
private:
	std::shared_ptr<ComponentManager> m_meshRendererManager;

public:

	RenderSystem(ECS& ecs) {
		m_meshRendererManager = ecs.getComponentManager<MeshRenderer>();
	}

	void render(ECS& ecs, ResourceManager& resourceManager, Shader& shader, Camera& camera, const std::vector<Light>& lights) {

		shader.activate();
		shader.setUniform("lightColor", lights[0].color);
		shader.setUniform("lightPos", lights[0].position);

		auto& componentArray = *m_meshRendererManager->getComponentArray<MeshRenderer>();


		for (size_t i = 0; i < componentArray.getSize(); i++)
		{
			MeshRenderer& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			resourceManager.getMesh(mr.mesh).Draw(shader, camera, t.position, t.rotation, t.scale, lights);
		}
	}
};