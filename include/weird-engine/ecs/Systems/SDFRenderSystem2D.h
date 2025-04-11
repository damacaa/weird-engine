#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"

namespace WeirdEngine
{
	using namespace ECS;

	class SDFRenderSystem2D : public System
	{
	public:

		SDFRenderSystem2D(ECSManager& ecs)
		{
			m_sdfRendererManager = ecs.getComponentManager<SDFRenderer>();
			m_customShapeManager = ecs.getComponentManager<CustomShape>();
			m_transformManager = ecs.getComponentManager<Transform>();
		}

		void fillDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size) 
		{ 
			auto componentArray = m_sdfRendererManager->getComponentArray();
			auto customShapeArray = m_customShapeManager->getComponentArray();
			auto transformArray = m_transformManager->getComponentArray();

			uint32_t ballCount = componentArray->getSize();
			uint32_t customShapeCount = customShapeArray->getSize();

			size = ballCount + (2 * customShapeCount);
			data = new WeirdRenderer::Dot2D[size];

			for (size_t i = 0; i < ballCount; i++)
			{
				auto& mr = componentArray->getDataAtIdx(i);
				auto& t = transformArray->getDataFromEntity(mr.Owner);

				data[i].position = (vec2)t.position;
				data[i].size = t.position.z;
				data[i].material = mr.materialId;
			}

			// 2 vec4s per customShape
			for (size_t i = 0; i < customShapeCount; i++)
			{
				auto& shape = customShapeArray->getDataAtIdx(i);

				data[ballCount + (2 * i)].position = vec2(shape.m_parameters[0], shape.m_parameters[1]);
				data[ballCount + (2 * i)].size = shape.m_parameters[2];
				data[ballCount + (2 * i)].material = shape.m_parameters[3];

				data[ballCount + (2 * i) + 1].position = vec2(shape.m_parameters[4], shape.m_parameters[5]);
				data[ballCount + (2 * i) + 1].size = shape.m_parameters[6];
				data[ballCount + (2 * i) + 1].material = shape.m_parameters[7];
			}
		}

		// lights are still not used
		void render(ECSManager& ecs, WeirdRenderer::Shader& shader, WeirdRenderer::RenderPlane& rp, const std::vector< WeirdRenderer::Light>& lights)
		{
			m_materialsAreDirty = true;
			if (m_materialsAreDirty)
			{
				shader.setUniform("u_staticColors", m_colorPalette, 16);
				m_materialsAreDirty = false;
			}

			WeirdRenderer::Dot2D* data;
			uint32_t size;

			fillDataBuffer(data, size);

			//shader.setUniform("directionalLightDirection", lights[0].rotation);
			rp.Draw(shader, data, size);

			delete[] data; // TODO: reuse data buffer
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

		bool shaderNeedsUpdate() const
		{
			return m_customShapesNeedUpdate;
		}

	private:
		std::shared_ptr<ComponentManager<SDFRenderer>> m_sdfRendererManager;
		std::shared_ptr<ComponentManager<CustomShape>> m_customShapeManager;
		std::shared_ptr<ComponentManager<Transform>> m_transformManager;

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
		bool m_customShapesNeedUpdate = true;
		size_t m_lastCustomShapeCount = 0;
	};
}