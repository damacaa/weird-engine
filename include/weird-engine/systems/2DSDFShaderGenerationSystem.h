#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"
#include "weird-engine/systems/SDFRenderSystem2D.h"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

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

		auto toGlslFloat = [](float value) {
			std::ostringstream ss;
			ss << std::fixed << std::setprecision(6) << value;
			return ss.str();
		};

		// ECMAScript std::regex does not support lookbehind assertions.
		static const std::regex integerLiteralRegex(R"((^|[^A-Za-z0-9_.])(\d+)([^A-Za-z0-9_.]|$))");

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
						<< " <= max(minDist, 0.0)){ finalMaterialId = currentGroupColor;}\n";
					// oss << "{ finalMaterialId = currentGroupColor;}\n";
					oss << "if(minDist >" << groupDistanceVariable << "){ minDist = " << groupDistanceVariable
						<< ";}\n";
				}

				// Next group
				currentGroup = group;
				groupDistanceVariable = "d" + std::to_string(currentGroup);

				// Initialize distance with big value
				oss << "float " << groupDistanceVariable << "= 10000.0;\n";
			}

			if (group == CustomShape::GLOBAL_GROUP - 1)
				break;

			// Start shape
			oss << "{\n";

			// Calculate data position in array
			oss << "int idx = dataOffset + " << 2 * i << ";\n";

			// Fetch parameters
			oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, ivec2(idx % 16384, idx / 16384), 0);\n";
			oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, ivec2((idx + 1) % 16384, (idx + 1) / 16384), 0);\n";

			// Get distance function
			auto fragmentCode = sdfs[shape.distanceFieldId]->print();
			fragmentCode = std::regex_replace(fragmentCode, integerLiteralRegex, "$1$2.0$3");

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;

			// GLES workaround: some drivers don't propagate default float precision
			// to array types (e.g. vec2[9]), causing compilation errors on mobile WebGL.
			// Extract inline array constructors into local variables with explicit precision.
			std::string arrayPreamble;
			{
				const std::string arrayPattern = "vec2[](";
				size_t arrayPos = fragmentCode.find(arrayPattern);
				if (arrayPos != std::string::npos)
				{
					// Find matching closing paren for the constructor
					size_t parenStart = arrayPos + arrayPattern.length() - 1;
					int depth = 0;
					size_t end = parenStart;
					for (; end < fragmentCode.size(); end++)
					{
						if (fragmentCode[end] == '(')
							depth++;
						else if (fragmentCode[end] == ')')
						{
							depth--;
							if (depth == 0)
							{
								end++;
								break;
							}
						}
					}

					// Parse individual vec2 elements from the constructor
					std::vector<std::string> elements;
					size_t elemStart = parenStart + 1;
					int elemDepth = 0;
					for (size_t k = elemStart; k < end - 1; k++)
					{
						if (fragmentCode[k] == '(')
							elemDepth++;
						else if (fragmentCode[k] == ')')
							elemDepth--;
						else if (fragmentCode[k] == ',' && elemDepth == 0)
						{
							elements.push_back(fragmentCode.substr(elemStart, k - elemStart));
							elemStart = k + 1;
						}
					}
					elements.push_back(fragmentCode.substr(elemStart, end - 1 - elemStart));

					// Replace inline constructor with variable reference
					fragmentCode.replace(arrayPos, end - arrayPos, "_sdfPoly");

					// Emit element-by-element initialization (avoids array constructor syntax)
					arrayPreamble =
						"highp vec2 _sdfPoly[" + std::to_string(elements.size()) + "];\n";
					for (size_t k = 0; k < elements.size(); k++)
					{
						arrayPreamble +=
							"_sdfPoly[" + std::to_string(k) + "] = " + elements[k] + ";\n";
					}
				}
			}

			// Shape distance calculation
			oss << arrayPreamble;
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
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist," << toGlslFloat(shape.smoothFactor)
						<< ");\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material
						<< ": currentGroupColor;" << std::endl;
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					// Smoothly subtract "dist" from currentMinDistance
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, " << toGlslFloat(shape.smoothFactor)
						<< ");\n";
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