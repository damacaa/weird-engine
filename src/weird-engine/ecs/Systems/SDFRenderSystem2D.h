#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"


class SDFRenderSystem2D : public System
{
private:
	std::shared_ptr<ComponentManager> m_sdfRendererManager;
	std::shared_ptr<ComponentManager> m_transformManager;

	glm::vec3 m_colorPalette[16] = {
			vec3(0.025f, 0.025f, 0.05f), // Black
			vec3(1.0f, 1.0f, 1.0f), // White
			vec3(0.484f, 0.484f, 0.584f), // Dark Gray
			vec3(0.752f, 0.762f, 0.74f), // Light Gray
			vec3(.95f, 0.1f, 0.1f), // Red
			vec3(0.1f, .95f, 0.1f), // Green
			vec3(0.15f, 0.25f, .85f), // Blue
			vec3(1.0f, .9f, 0.2f), // Yellow
			vec3(.95f, 0.4f, 0.1f), // Orange
			vec3(0.5f, 0.0f, 1.0f), // Purple
			vec3(0.0f, .9f, .9f), // Cyan
			vec3(1.0f, 0.3f, .6f), // Magenta
			vec3(0.5f, 1.0f, 0.5f), // Light Green
			vec3(1.0f, 0.5f, 0.5f), // Pink
			vec3(0.5f, 0.5f, 1.0f), // Light Blue
			vec3(0.4f, 0.25f, 0.1f) // Brown
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
		if (m_materialsAreDirty) 
		{
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
			data[i].parameters = glm::vec4(1.0f);
		}

		//shader.setUniform("directionalLightDirection", lights[0].rotation);
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