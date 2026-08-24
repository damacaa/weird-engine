#pragma once

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Input.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/resources/Shader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace WeirdEngine::SDFShaderGenerationSystem
{
	template <typename ShapeClass, typename RenderContext>
	inline void update(Registry& registry, RenderContext& ctx, WeirdRenderer::Shader& shader,
					   const std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		const auto componentArray = registry.getComponentManager<ShapeClass>()->getComponentArray();

		if (!ctx.shapesNeedUpdate)
		{
			return;
		}

		std::cout << "Updating shader code for: " << std::string(typeid(ShapeClass).name()) << "\n";

		ctx.shapesNeedUpdate = false;

		auto toGlslFloat = [](float value)
		{
			std::ostringstream ss;
			ss << std::fixed << std::setprecision(6) << value;
			return ss.str();
		};

		std::ostringstream oss;
		std::ostringstream functionsOss;

		oss << "///////////////////////////////////////////\n";
		oss << "int dataOffset = u_loadedObjects - (2 * u_customShapeCount);\n";
		oss << "int currentGroupColor = -1;\n";

		int currentGroup = -1;
		std::string groupDistanceVariable;
		std::string groupBlendVariable;

		ShapeClass dummyShape;
		dummyShape.groupIdx = CustomShape::GLOBAL_GROUP - 1;

		std::vector<size_t> orderedIndices;
		orderedIndices.reserve(componentArray->getSize());

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			orderedIndices.push_back(i);
		}

		std::stable_sort(orderedIndices.begin(), orderedIndices.end(),
						 [&](size_t a, size_t b) {
							 return componentArray->getDataAtIdx(a).groupIdx < componentArray->getDataAtIdx(b).groupIdx;
						 });

		for (size_t idx = 0; idx < componentArray->getSize() + 1; idx++)
		{
			size_t i = idx == componentArray->getSize() ? 0 : orderedIndices.at(idx);
			const ShapeClass& shape = idx == componentArray->getSize() ? dummyShape : componentArray->getDataAtIdx(i);
			const int group = shape.groupIdx;

			if (group != currentGroup)
			{
				if (currentGroup != -1)
				{
					oss << "if(" << groupDistanceVariable
						<< " <= max(minDist, 0.0)){ finalMaterialId = currentGroupColor;}\n";
					oss << "if(" << groupDistanceVariable << " <= minDist) { globalBlend = " << groupBlendVariable
						<< "; }\n";
					oss << "if(minDist > " << groupDistanceVariable << "){ minDist = " << groupDistanceVariable
						<< ";}\n";
				}

				currentGroup = group;
				groupDistanceVariable = "d" + std::to_string(currentGroup);
				groupBlendVariable = "b" + std::to_string(currentGroup);
				oss << "float " << groupDistanceVariable << " = 10000.0;\n";
				oss << "float " << groupBlendVariable << " = 0.0;\n";
			}

			if (group == CustomShape::GLOBAL_GROUP - 1)
			{
				break;
			}

			oss << "{\n";
			oss << "int idx = dataOffset + " << 2 * i << ";\n";

			// Fetch parameters
			oss << "vec4 parameters0 = texelFetch(t_shapeBuffer, ivec2(idx % 16384, idx / 16384), 0);\n";
			oss << "vec4 parameters1 = texelFetch(t_shapeBuffer, ivec2((idx + 1) % 16384, (idx + 1) / 16384), 0);\n";

			auto fragmentCode = sdfs[shape.distanceFieldId]->print();

			// Replace integer literals with floats manually to avoid std::regex ABI issues
			std::string resultStr;
			resultStr.reserve(fragmentCode.size() * 2);
			for (size_t k = 0; k < fragmentCode.size();)
			{
				bool isPrevValid = (k == 0) || (!std::isalnum(fragmentCode[k - 1]) && fragmentCode[k - 1] != '_' &&
												fragmentCode[k - 1] != '.');

				if (isPrevValid && std::isdigit(fragmentCode[k]))
				{
					size_t start = k;
					while (k < fragmentCode.size() && std::isdigit(fragmentCode[k]))
						k++;

					bool isNextValid = (k == fragmentCode.size()) || (!std::isalnum(fragmentCode[k]) &&
																	  fragmentCode[k] != '_' && fragmentCode[k] != '.');

					resultStr.append(fragmentCode, start, k - start);

					if (isNextValid)
						resultStr.append(".0");
				}
				else
				{
					resultStr.push_back(fragmentCode[k]);
					k++;
				}
			}
			fragmentCode = std::move(resultStr);

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;

			std::string helperFunctions = sdfs[shape.distanceFieldId]->getHelperFunctions();
			functionsOss << helperFunctions;
			oss << "float dist = " << fragmentCode << ";\n";

			// 3D shader uses this to apply dithering to the distance of shapes with transparent materials, this creates
			// paterns where the shape is partially rendered, which creates the illusion of transparency without needing
			// to sort objects or use alpha blending, which can be costly in raymarching shaders. The 2D shader ignores
			// this step for now, but it could be used in the future if we decide to add transparency to 2D shapes as
			// well.
			oss << "dist = modifyDistanceBasedOnMaterial(dist, " << shape.material << ", idx);\n";

			oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";\n";
			oss << "float currentBlend = " << (globalEffect ? "globalBlend" : groupBlendVariable) << ";\n";

			switch (shape.combination)
			{
				case CombinationType::Addition:
				{
					oss << "if (dist <= currentMinDistance) { currentBlend = 0.0; }\n";
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "currentGroupColor = dist <= currentMinDistance ? " << shape.material
						<< " : currentGroupColor;\n";
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
					oss << "vec2 res = fOpUnionSoft_blend(currentMinDistance, dist, " << toGlslFloat(shape.smoothFactor)
						<< ");\n";
					oss << "if (res.y > 0.0) { currentBlend = max(currentBlend, res.y); }\n";
					oss << "else if (dist < currentMinDistance) { currentBlend = 0.0; }\n";
					oss << "currentMinDistance = res.x;\n";
					oss << "currentGroupColor = dist <= currentMinDistance ? " << shape.material
						<< " : currentGroupColor;\n";
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					oss << "currentMinDistance = fOpSubSoft(currentMinDistance, dist, "
						<< toGlslFloat(shape.smoothFactor) << ");\n";
					break;
				}
				default:
					break;
			}

			oss << (globalEffect ? "minDist" : groupDistanceVariable) << " = currentMinDistance;\n";
			oss << (globalEffect ? "globalBlend" : groupBlendVariable) << " = currentBlend;\n";
			oss << "}\n\n";
		}

		std::string replacement = oss.str();
		std::string helperFunctionsTotal = functionsOss.str();
		if (!helperFunctionsTotal.empty())
			WeirdEngine::Logger::log("HELPER FUNCTIONS TOTAL:\n" + helperFunctionsTotal);

		shader.setFragmentIncludeCode(0, helperFunctionsTotal, false);
		shader.setFragmentIncludeCode(1, replacement, true);

#ifndef NDEBUG
		if (true)
		{
			WeirdEngine::Logger::log(replacement);

			static int shaderDumpId = 0;
			std::ofstream outFile("generated_shader_" + std::string(typeid(ShapeClass).name()) + "_" +
								  std::to_string(shaderDumpId++) + ".frag");
			if (outFile.is_open())
			{
				outFile << shader.getFragmentCode();
				outFile.close();
			}
		}
#endif
	}
} // namespace WeirdEngine::SDFShaderGenerationSystem