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

		void updateCustomShapesShader(WeirdRenderer::Shader& shader, std::vector<std::shared_ptr<IMathExpression>> sdfs)
		{
			const auto sdfBalls = m_dotClassManager->getComponentArray();
			int32_t ballsCount = sdfBalls->getSize();
			const auto componentArray = m_shapeClassManager->getComponentArray();
			shader.setUniform("u_customShapeCount", componentArray->getSize());

			if (!m_shapesNeedUpdate)
			{
				return;
			}

			m_shapesNeedUpdate = false;

			std::ostringstream oss;

			oss << "///////////////////////////////////////////\n";

			oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);\n";

			int currentGroup = -1;
			std::string groupDistanceVariable;

			for (size_t i = 0; i < componentArray->getSize(); i++)
			{
				// Get shape
				const auto& shape = componentArray->getDataAtIdx(i);

				// Get group
				const int group = shape.m_groupId;

				// Start new group if necessary
				if (group != currentGroup)
				{
					// If this is not the first group, combine current group distance with global minDistance
					if (currentGroup != -1)
					{
						oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
					}

					// Next group
					currentGroup = group;
					groupDistanceVariable = "d" + std::to_string(currentGroup);

					// Initialize distance with big value
					oss << "float " << groupDistanceVariable << "= 10000;\n";
				}

				// Start shape
				oss << "{\n";

				// Calculate data position in array
				oss << "int idx = dataOffset + " << 2 * i << ";\n";

				// Fetch parameters
				oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, idx);\n";
				oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, idx + 1);\n";

				// Get distance function
				auto fragmentCode = sdfs[shape.m_distanceFieldId]->print();

				// Use screen space coords (DEPRECATED)
				if (shape.m_screenSpace)
				{
					replaceSubstring(fragmentCode, "var9", "var11");
					replaceSubstring(fragmentCode, "var10", "var12");
				}

				bool globalEffect = group == CustomShape::GLOBAL_GROUP;

				// Shape distance calculation
				oss << "float dist = " << fragmentCode << ";" << std::endl;

				// Apply globalEffect logic
				oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";" << std::endl;

				// Combine shape distance
				switch (shape.m_combination)
				{
				case CombinationType::Addition:
				{
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? " << shape.m_material << ": finalMaterialId;" << std::endl;
					break;
				}
				case CombinationType::Subtraction:
				{
					oss << "currentMinDistance = max(currentMinDistance, -dist);\n";
					break;
				}
				case CombinationType::Intersection:
				{
					oss << "currentMinDistance = max(currentMinDistance, dist);\n";
					break;
				}
				case CombinationType::SmoothAddition:
				{
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist, 1.0);\n";
					oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? " << shape.m_material << ": finalMaterialId;" << std::endl;
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					// Smoothly subtract "dist" from currentMinDistance
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, 1.0);\n";
					// Material belongs to the *a* shape if it still “wins” after subtraction
					// oss << "finalMaterialId = dist <= min(minDist, currentMinDistance) ? "
					//	<< shape.m_material << " : finalMaterialId;" << std::endl;
					break;
				}
				default:
					break;
				}

				// Assign back to the correct target
				oss << (globalEffect ? "minDist" : groupDistanceVariable) << " = currentMinDistance;\n";
				oss << "}\n\n";
			}

			// Combine last group
			if (componentArray->getSize() > 0)
			{
				oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
			}

			// Get string
			std::string replacement = oss.str();

			// Set new source code and recompile shader
			shader.setFragmentIncludeCode(1, replacement);

#ifndef NDEBUG
			if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKey(Input::R))
			{
				std::cout << replacement << std::endl;

				// Broken
				std::ofstream outFile("generated_shader.frag");
				if (outFile.is_open()) {
					outFile << shader.getFragmentCode();
					outFile.close();
				}
			}
#endif
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