#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"


class SDFRenderSystem2D : public System
{
private:
	std::shared_ptr<ComponentManager> m_sdfRendererManager;
	std::shared_ptr<ComponentManager> m_transformManager;

	glm::vec3 m_colorPalette[16] = {
		glm::vec3(0.0, 0.9, 0.1),
		glm::vec3(1.0, 0.05, 0.01),
		glm::vec3(0.1, 0.05, 0.80),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0),
		glm::vec3(0.0, 0.0, 0.0)
	};

	bool m_materialsAreDirty = true;

public:
	SDFRenderSystem2D(ECSManager& ecs)
	{
		m_sdfRendererManager = ecs.getComponentManager<SDFRenderer>();
		m_transformManager = ecs.getComponentManager<Transform>();
	}

	void render(ECSManager& ecs, WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp, const std::vector< WeirdRenderer::Light>& lights)
	{
		if (m_materialsAreDirty) {

			shader.setUniform("u_staticColors", m_colorPalette, 16);
			m_materialsAreDirty = false;
		}

		auto& componentArray = *m_sdfRendererManager->getComponentArray<SDFRenderer>();
		auto& transformArray = *m_transformManager->getComponentArray<Transform>();

		unsigned int size = componentArray.getSize();

		WeirdRenderer::Shape2D* data = new  WeirdRenderer::Shape2D[size];

		for (size_t i = 0; i < size; i++)
		{
			auto& mr = componentArray[i];
			auto& t = transformArray.getData(mr.Owner);

			data[i].position = t.position;
			data[i].material = mr.materialId;
			data[i].parameters = glm::vec4(i);
		}

		shader.setUniform("directionalLightDirection", lights[0].rotation);
		rp.Draw(shader, data, size);

		delete[] data;
	}
};