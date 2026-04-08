#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"
#include "weird-engine/systems/SDFRenderSystem2D.h"
#include <memory>

namespace WeirdEngine::SDFShaderGenerationSystem2D
{

	// Update shader code with SDF data
	template <typename DotClass, typename ShapeClass>
	inline void update(ECSManager& ecs, SDFRenderSystem2DContext& ctx, WeirdRenderer::Shader& shader,
					   std::vector<std::shared_ptr<IMathExpression>> sdfs)
	{
		const auto sdfBalls = ecs.getComponentManager<DotClass>()->getComponentArray();
		const auto componentArray = ecs.getComponentManager<ShapeClass>()->getComponentArray();

		if (!ctx.m_shapesNeedUpdate)
		{
			return;
		}

		ctx.m_shapesNeedUpdate = false;

		std::ostringstream oss;

		oss << "///////////////////////////////////////////\n";

		oss << "int dataOffset =  u_loadedObjects - (2 * u_customShapeCount);\n";

		oss << "int currentGroupColor = -1;\n";

		int currentGroup = -1;
		std::string groupDistanceVariable;

		ShapeClass dummyShape;
		dummyShape.groupIdx = CustomShape::GLOBAL_GROUP - 1;

		// TODO: do this for physics too
		// Sort Shapes by group
		std::vector<size_t> orderedIndices;
		orderedIndices.reserve(componentArray->getSize());

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			orderedIndices.push_back(i);
		}

		// Sort indices by the shape's group value
		std::sort(orderedIndices.begin(), orderedIndices.end(), [&](size_t a, size_t b)
				  { return componentArray->getDataAtIdx(a).groupIdx < componentArray->getDataAtIdx(b).groupIdx; });

		for (size_t idx = 0; idx < componentArray->getSize() + 1; idx++)
		{
			size_t i = idx == componentArray->getSize() ? 0 : orderedIndices.at(idx);

			// Get shape
			const ShapeClass& shape = idx == componentArray->getSize() ? dummyShape : componentArray->getDataAtIdx(i);

			// Get group
			const int group = shape.groupIdx;

			// Start new group if necessary
			if (group != currentGroup)
			{
				// If this is not the first group, combine current group distance with global minDistance
				if (currentGroup != -1)
				{
					oss << "if(" << groupDistanceVariable
						<< " <= max(minDist, 0)){ finalMaterialId = currentGroupColor;}\n";
					// oss << "{ finalMaterialId = currentGroupColor;}\n";
					oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable
						<< ";}\n";
				}

				// Next group
				currentGroup = group;
				groupDistanceVariable = "d" + std::to_string(currentGroup);

				// Initialize distance with big value
				oss << "float " << groupDistanceVariable << "= 10000;\n";
			}

			if (group == CustomShape::GLOBAL_GROUP - 1)
				break;

			// Start shape
			oss << "{\n";

			// Calculate data position in array
			oss << "int idx = dataOffset + " << 2 * i << ";\n";

			// Fetch parameters
			oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, ivec2(idx, 0), 0);\n";
			oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, ivec2(idx + 1, 0), 0);\n";

			// Get distance function
			auto fragmentCode = sdfs[shape.distanceFieldId]->print();

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;

			// Shape distance calculation
			oss << "float dist = " << fragmentCode << ";" << std::endl;

			// oss << "#ifdef ORIGIN_AT_BOTTOM_LEFT" << std::endl;
			// oss << "float pixelSize = 10.0; dist = abs(dist - pixelSize) - (pixelSize);" << std::endl;
			// oss << "#endif" << std::endl;

			// Apply globalEffect logic
			oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";"
				<< std::endl;

			// Combine shape distance
			switch (shape.combination)
			{
				case CombinationType::Addition:
				{
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material
						<< ": currentGroupColor;" << std::endl;
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
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist," << shape.smoothFactor
						<< ");\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material
						<< ": currentGroupColor;" << std::endl;
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					// Smoothly subtract "dist" from currentMinDistance
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, " << shape.smoothFactor << ");\n";
					// Material belongs to the *a* shape if it still �wins� after subtraction
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
		// if (componentArray->getSize() > 0)
		// {
		// 	oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable << ";}\n";
		// }

		// Get string
		std::string replacement = oss.str();

		// Set new source code and recompile shader
		shader.setFragmentIncludeCode(1, replacement);

		// std::cout << replacement << std::endl;

#ifndef NDEBUG
		if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKey(Input::R))
		{
			std::cout << replacement << std::endl;

			// Broken
			std::ofstream outFile("generated_shader.frag");
			if (outFile.is_open())
			{
				outFile << shader.getFragmentCode();
				outFile.close();
			}
		}
#endif
	}

} // namespace WeirdEngine::SDFShaderGenerationSystem2D