#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"

class SDFRenderSystem2D : public System
{
private:
	std::shared_ptr<ComponentManager> m_sdfRendererManager;
	std::shared_ptr<ComponentManager> m_transformManager;

public:
	SDFRenderSystem2D(ECS &ecs)
	{
		m_sdfRendererManager = ecs.getComponentManager<SDFRenderer>();
		m_transformManager = ecs.getComponentManager<Transform>();
	}

	void render(ECS &ecs, Shader &shader, RenderPlane &rp, const std::vector<Light> &lights)
	{

		auto &componentArray = *m_sdfRendererManager->getComponentArray<SDFRenderer>();
		auto &transformArray = *m_transformManager->getComponentArray<Transform>();

		unsigned int size = componentArray.getSize();

		Shape2D *data = new Shape2D[size];

		for (size_t i = 0; i < size; i++)
		{
			auto &mr = componentArray[i];
			auto &t = transformArray.getData(mr.Owner);

			data[i].position = t.position;
			data[i].material = mr.materialId;
			data[i].parameters = glm::vec4(i);
		}

		shader.setUniform("directionalLightDirection", lights[0].rotation);
		rp.Draw(shader, data, size);

		delete[] data;
	}
};