#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"

class SDFRenderSystem : public System {
private:
	std::shared_ptr<ComponentManager> m_sdfRendererManager;
	std::shared_ptr<ComponentManager> m_transformManager;

public:

	SDFRenderSystem(ECSManager& ecs) {
		m_sdfRendererManager = ecs.getComponentManager<SDFRenderer>();
		m_transformManager = ecs.getComponentManager<Transform>();
	}

	void render(ECSManager& ecs, WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp, const std::vector< WeirdRenderer::Light>& lights) {

		auto& componentArray = *m_sdfRendererManager->getComponentArray<SDFRenderer>();
		auto& transformArray = *m_transformManager->getComponentArray<Transform>();

		unsigned int size = componentArray.getSize();

		WeirdRenderer::Shape* data = new WeirdRenderer::Shape[size];

		for (size_t i = 0; i < size; i++)
		{
			auto& mr = componentArray[i];
			//auto& t = ecs.getComponent<Transform>(mr.Owner);
			auto& t = transformArray.getData(mr.Owner);

			data[i].position = t.position;// +glm::vec3(0, mr.Owner, 0);
			//data[i].size = t.scale.x;
		}

		shader.setUniform("directionalLightDirection", lights[0].rotation);
		rp.Draw(shader, data, size);

		delete[] data;

		//rp->Draw(shader, m_data, m_shapes);
	}
};