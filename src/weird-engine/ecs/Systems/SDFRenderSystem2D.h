#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"


class SDFRenderSystem2D : public System
{
private:
	std::shared_ptr<ComponentManager> m_sdfRendererManager;
	std::shared_ptr<ComponentManager> m_transformManager;

	glm::vec3 m_colorPalette[16] = {
		glm::vec3(0.0f, 0.0f, 0.0f), // Black
		glm::vec3(1.0f, 1.0f, 1.0f), // White
		glm::vec3(0.584f, 0.584f, 0.584f), // Dark Gray
		glm::vec3(0.792f, 0.792f, 0.792f), // Light Gray
		glm::vec3(1.0f, 0.0f, 0.0f), // Red
		glm::vec3(0.0f, 1.0f, 0.0f), // Green
		glm::vec3(0.0f, 0.0f, 1.0f), // Blue
		glm::vec3(1.0f, 1.0f, 0.0f), // Yellow
		glm::vec3(1.0f, 0.5f, 0.0f), // Orange
		glm::vec3(0.5f, 0.0f, 1.0f), // Purple
		glm::vec3(0.0f, 1.0f, 1.0f), // Cyan
		glm::vec3(1.0f, 0.0f, 1.0f), // Magenta
		glm::vec3(0.5f, 1.0f, 0.5f), // Light Green
		glm::vec3(1.0f, 0.5f, 0.5f), // Pink
		glm::vec3(0.5f, 0.5f, 1.0f), // Light Blue
		glm::vec3(0.5f, 0.25f, 0.0f) // Brown
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

	// Function to find the closest color in the palette
	int findClosestColorInPalette(const glm::vec3& color) {
		int closestIndex = 0;
		float minDistance = std::numeric_limits<float>::max(); // start with maximum possible distance

		for (int i = 0; i < 16; ++i) {
			// Use length2 for efficiency (avoids computing square root)
			float distance = glm::length2(color - m_colorPalette[i]);

			if (distance < minDistance) {
				minDistance = distance;
				closestIndex = i;
			}
		}

		return closestIndex;
	}
};