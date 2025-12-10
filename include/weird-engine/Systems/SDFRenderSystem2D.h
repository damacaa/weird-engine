#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ecs/Component.h"

#include <memory>
#include <limits>

namespace WeirdEngine
{
	using namespace ECS;

	template<typename DotClass, typename ShapeClass>
	class SDFRenderSystem2D : public System
	{
	public:
		SDFRenderSystem2D(ECSManager& ecs)
		{
			m_dotClassManager = ecs.getComponentManager<DotClass>();
			m_shapeClassManager = ecs.getComponentManager<ShapeClass>();
			m_transformManager = ecs.getComponentManager<Transform>(); // Transform remains non-templated
		}

		void fillDataBuffer(WeirdRenderer::Dot2D*& data, uint32_t& size)
		{
			// Get component arrays for the templated types
			auto dotClassArray = m_dotClassManager->getComponentArray();
			auto shapeClassArray = m_shapeClassManager->getComponentArray();
			auto transformArray = m_transformManager->getComponentArray();

			uint32_t dotCount = dotClassArray->getSize();
			uint32_t shapeCount = shapeClassArray->getSize();

			// Each ShapeClass will contribute 2 dots to the buffer
			uint32_t newSize = dotCount + (2 * shapeCount);

			if (size != newSize)
			{
				if (data)
				{
					delete[] data;
					data = nullptr;
				}

				size = newSize;
			}

			if (!data && size > 0)
			{
				data = new WeirdRenderer::Dot2D[size];
			}

			// Process DotClass instances
			for (size_t i = 0; i < dotCount; i++)
			{
				auto& dotComp = dotClassArray->getDataAtIdx(i);
				auto& t = transformArray->getDataFromEntity(dotComp.Owner); // Assuming DotClass has an 'Owner' member

				data[i].position = (glm::vec2)t.position;
				data[i].size = t.position.z;
				data[i].material = dotComp.materialId;
			}

			// Process ShapeClass instances
			for (size_t i = 0; i < shapeCount; i++)
			{
				auto& shapeComp = shapeClassArray->getDataAtIdx(i);

				// Assuming ShapeClass has m_parameters[0] through m_parameters[7]
				// Make sure your ShapeClass provides these members.
				data[dotCount + (2 * i)].position = glm::vec2(shapeComp.m_parameters[0], shapeComp.m_parameters[1]);
				data[dotCount + (2 * i)].size = shapeComp.m_parameters[2];
				data[dotCount + (2 * i)].material = shapeComp.m_parameters[3];

				data[dotCount + (2 * i) + 1].position = glm::vec2(shapeComp.m_parameters[4], shapeComp.m_parameters[5]);
				data[dotCount + (2 * i) + 1].size = shapeComp.m_parameters[6];
				data[dotCount + (2 * i) + 1].material = shapeComp.m_parameters[7];
			}
		}

		void updatePalette(WeirdRenderer::Shader& shader)
		{
			m_materialsAreDirty = true;
			if (m_materialsAreDirty)
			{
				shader.setUniform("u_staticColors", m_colorPalette, 16);
				m_materialsAreDirty = false;
			}
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

		bool& shaderNeedsUpdate()
		{
			return m_shapesNeedUpdate;
		}

	private:
		std::shared_ptr<ComponentManager<DotClass>> m_dotClassManager;
		std::shared_ptr<ComponentManager<ShapeClass>> m_shapeClassManager;
		std::shared_ptr<ComponentManager<Transform>> m_transformManager;

		glm::vec3 m_colorPalette[16] = {
			glm::vec3(0.025f, 0.025f, 0.05f), // Black
			glm::vec3(1.0f, 1.0f, 1.0f), // White
			glm::vec3(0.484f, 0.484f, 0.584f), // Dark Gray
			glm::vec3(0.752f, 0.762f, 0.74f), // Light Gray
			glm::vec3(.95f, 0.1f, 0.1f), // Red
			glm::vec3(0.1f, .95f, 0.1f), // Green
			glm::vec3(0.15f, 0.25f, .85f), // Blue
			glm::vec3(1.0f, .9f, 0.2f), // Yellow
			glm::vec3(.95f, 0.4f, 0.1f), // Orange
			glm::vec3(0.5f, 0.0f, 1.0f), // Purple
			glm::vec3(0.0f, .9f, .9f), // Cyan
			glm::vec3(1.0f, 0.3f, .6f), // Magenta
			glm::vec3(0.5f, 1.0f, 0.5f), // Light Green
			glm::vec3(1.0f, 0.5f, 0.5f), // Pink
			glm::vec3(0.5f, 0.5f, 1.0f), // Light Blue
			glm::vec3(0.4f, 0.25f, 0.1f) // Brown
		};

		bool m_materialsAreDirty = true;
		bool m_shapesNeedUpdate = true;
		size_t m_lastShapeCount = 0;
	};
}