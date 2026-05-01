  #pragma once

#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/resources/Shader.h"

#include <algorithm>
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
	template <typename ShapeClass>
	inline void updateShaderCode(bool& shapesNeedUpdate, ECSManager& ecs, WeirdRenderer::Shader& shader,
							 const std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		const auto componentArray = ecs.getComponentManager<ShapeClass>()->getComponentArray();

		if (!shapesNeedUpdate)
		{
			return;
		}

		shapesNeedUpdate = false;

		auto toGlslFloat = [](float value) {
			std::ostringstream ss;
			ss << std::fixed << std::setprecision(6) << value;
			return ss.str();
		};

		static const std::regex integerLiteralRegex(R"((^|[^A-Za-z0-9_.])(\d+)([^A-Za-z0-9_.]|$))");

		std::ostringstream oss;

		oss << "///////////////////////////////////////////\n";
		oss << "int dataOffset = u_loadedObjects - (2 * u_customShapeCount);\n";
		oss << "int currentGroupColor = -1;\n";

		int currentGroup = -1;
		std::string groupDistanceVariable;

		ShapeClass dummyShape;
		dummyShape.groupIdx = CustomShape::GLOBAL_GROUP - 1;

		std::vector<size_t> orderedIndices;
		orderedIndices.reserve(componentArray->getSize());

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			orderedIndices.push_back(i);
		}

		std::sort(orderedIndices.begin(), orderedIndices.end(), [&](size_t a, size_t b)
				  { return componentArray->getDataAtIdx(a).groupIdx < componentArray->getDataAtIdx(b).groupIdx; });

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
					oss << "if(minDist > " << groupDistanceVariable << "){ minDist = " << groupDistanceVariable
						<< ";}\n";
				}

				currentGroup = group;
				groupDistanceVariable = "d" + std::to_string(currentGroup);
				oss << "float " << groupDistanceVariable << " = 10000.0;\n";
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
			fragmentCode = std::regex_replace(fragmentCode, integerLiteralRegex, "$1$2.0$3");

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;

			std::string arrayPreamble;
			{
				const std::string arrayPattern = "vec2[](";
				size_t arrayPos = fragmentCode.find(arrayPattern);
				if (arrayPos != std::string::npos)
				{
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

					fragmentCode.replace(arrayPos, end - arrayPos, "_sdfPoly");

					arrayPreamble = "highp vec2 _sdfPoly[" + std::to_string(elements.size()) + "];\n";
					for (size_t k = 0; k < elements.size(); k++)
					{
						arrayPreamble += "_sdfPoly[" + std::to_string(k) + "] = " + elements[k] + ";\n";
					}
				}
			}

			oss << arrayPreamble;
			oss << "float dist = " << fragmentCode << ";\n";
			
      // 3D shader uses this to apply dithering to the distance of shapes with transparent materials, this creates paterns where the shape is partially rendered, which creates the illusion of transparency without needing to sort objects or use alpha blending, which can be costly in raymarching shaders. The 2D shader ignores this step for now, but it could be used in the future if we decide to add transparency to 2D shapes as well.
			oss << "dist = modifyDistanceBasedOnMaterial(dist, " << shape.material << ", idx);\n";

			oss << "float currentMinDistance = " << (globalEffect ? "minDist" : groupDistanceVariable) << ";\n";

			switch (shape.combination)
			{
				case CombinationType::Addition:
				{
					oss << "currentMinDistance = min(currentMinDistance, dist);\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material
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
					oss << "currentMinDistance = fOpUnionSoft(currentMinDistance, dist, "
						<< toGlslFloat(shape.smoothFactor) << ");\n";
					oss << "currentGroupColor = dist <= min(currentMinDistance, dist) ? " << shape.material
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
			oss << "}\n\n";
		}

		std::string replacement = oss.str();
		shader.setFragmentIncludeCode(1, replacement);

#ifndef NDEBUG
		if (Input::GetKey(Input::LeftCtrl) && Input::GetKey(Input::LeftShift) && Input::GetKey(Input::R))
		{
			std::cout << replacement << std::endl;

			std::ofstream outFile("generated_shader.frag");
			if (outFile.is_open())
			{
				outFile << shader.getFragmentCode();
				outFile.close();
			}
		}
#endif
	}

	template <typename ShapeClass, typename RenderContext>
	inline void update(ECSManager& ecs, RenderContext& ctx, WeirdRenderer::Shader& shader,
					   const std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		updateShaderCode<ShapeClass>(ctx.m_shapesNeedUpdate, ecs, shader, sdfs);
	}
} // namespace WeirdEngine::SDFShaderGenerationSystem