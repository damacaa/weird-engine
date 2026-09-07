#pragma once

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Input.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/resources/Shader.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// #define LOG_SDF_SHADER_GENERATION

/**
 * @file SDFShaderGenerationSystem.h
 * @brief Dynamic GLSL Shader Code Generation and Optimization for Signed Distance Fields (SDFs).
 *
 * =================================================================================================
 * ARCHITECTURE OVERVIEW & SHADER INTEGRATION PIPELINE
 * =================================================================================================
 *
 * 1. CPU-Side Representation (AST & ECS):
 *    - Each distinct geometric shape is defined in C++ as an AST (Abstract Syntax Tree) of mathematical expressions
 *      inheriting from `IMathExpression` (e.g., SDF primitives, unions, arithmetic operations).
 *    - Entities in the ECS registry hold a `CustomShape` (World) or `UIShape` (UI) component.
 *    - Each component stores:
 *        * `distanceFieldId`: Index identifying which AST to evaluate.
 *        * `parameters[8]`: Dynamic per-instance data (position, scale, rotation, animation vars).
 *        * `groupIdx`: Groups shapes that blend or combine together before updating global distance.
 *        * `material`: Material/color palette index.
 *        * `combination`: CSG operation (Addition, Subtraction, Intersection, SmoothAddition, etc.).
 *        * `smoothFactor`: Blend radius parameter used for smooth CSG unions/subtractions.
 *
 * 2. Shader Injection Points:
 *    The fragment shaders (`sdf_distance.frag` for 2D, `sdf_raymarching.frag` for 3D) contain
 *    two modular injection hooks replaced at runtime when scene shapes change:
 *
 *    a) Slot 0: `#include "helper_functions"`
 *       - Receives all GLSL helper routines:
 *           * `fetchShapeParams(...)` to read instance parameters from `t_shapeBuffer`.
 *           * `applyShapeAddition(...)`, `applyShapeSmoothAddition(...)`, etc. for CSG math.
 *           * `flushShapeGroup(...)` for group-level distance and material accumulation.
 *           * Unique helper functions required by specific AST nodes (e.g., `sdTriangle_impl`).
 *           * Dedicated `evaluate_sdf_<id>(p, parameters0, parameters1)` functions for each unique shape.
 *
 *    b) Slot 1: `#include "custom_shapes"`
 *       - Injected inside `getDistanceMaterialMask(p, uv)` in the fragment shader.
 *       - Contains the streamlined per-instance dispatch loop:
 *           * Fetches parameters from texture memory (`t_shapeBuffer`).
 *           * Calls `evaluate_sdf_<id>(p, p0, p1)`.
 *           * Applies the CSG combination and material tracking.
 *           * Flushes group bounds into global `minDist` and `finalMaterialId`.
 *
 * 3. Frontend Optimizations:
 *    - Constant Folding: Handled during AST construction in `MathExpressions.h`.
 *    - Common Subexpression Elimination (CSE): Deduplicates recurring subtrees into local GLSL variables.
 *    - Fixed-Precision Literals: Float constants are printed as exact GLSL literals (`0.500000`),
 *      eliminating runtime string patching and maintaining 100% compile-time purity.
 */

namespace WeirdEngine::SDFShaderGenerationSystem
{
	/**
	 * @namespace CSE
	 * @brief Common Subexpression Elimination for AST-to-GLSL code generation.
	 *
	 * Detects duplicate sub-trees in an expression graph and extracts them into local GLSL
	 * `float _vN = ...;` statements in strict dependency order before the `return` statement.
	 */
	namespace CSE
	{
		/**
		 * @brief Metadata tracking subtree usage and direct child dependencies.
		 */
		struct NodeInfo
		{
			std::shared_ptr<IMathExpression> expr; ///< Pointer to the AST expression node.
			std::vector<std::string> childSigs;	   ///< Canonical GLSL signatures of direct children.
			int rawCount = 0;					   ///< Total appearances across the AST.
		};

		/**
		 * @brief CSE compilation context collecting nodes in post-order traversal.
		 */
		struct Context
		{
			std::unordered_map<std::string, NodeInfo> nodes;
			std::vector<std::string> postOrder; ///< Bottom-up traversal order ensuring children precede parents.
		};

		/**
		 * @brief Traverses the AST in post-order, gathering unique subtrees and raw usage counts.
		 */
		inline std::string collectSubtrees(const std::shared_ptr<IMathExpression>& expr, Context& ctx)
		{
			if (!expr)
				return "";

			std::vector<std::shared_ptr<IMathExpression>> children;
			expr->getChildren(children);

			std::vector<std::string> childSigs;
			childSigs.reserve(children.size());
			for (const auto& child : children)
			{
				childSigs.push_back(collectSubtrees(child, ctx));
			}

			// Generate canonical signature: trivial nodes (variables/constants) use raw print(),
			// non-trivial nodes use their operator representation with child signatures.
			std::string sig = expr->isTrivial() ? expr->print() : expr->printWithChildren(childSigs);

			auto it = ctx.nodes.find(sig);
			if (it == ctx.nodes.end())
			{
				NodeInfo info;
				info.expr = expr;
				info.childSigs = std::move(childSigs);
				info.rawCount = 1;
				ctx.nodes[sig] = std::move(info);
				ctx.postOrder.push_back(sig);
			}
			else
			{
				it->second.rawCount++;
			}

			return sig;
		}

		/**
		 * @brief Computes effective evaluation counts by propagating usage downwards from root.
		 *
		 * When an outer parent expression is extracted into a variable, all its internal children
		 * are evaluated only once inside the variable's definition. Duplicate occurrences of the parent
		 * in the rest of the AST become variable references and do not re-evaluate the children.
		 */
		inline void computeEffectiveCounts(const std::string& rootSig, const Context& ctx,
										   const std::unordered_set<std::string>& extracted,
										   std::unordered_map<std::string, int>& effectiveCounts)
		{
			effectiveCounts.clear();
			effectiveCounts[rootSig] = 1;

			// Each extracted node definition evaluates its direct children once
			for (const auto& sig : extracted)
			{
				auto it = ctx.nodes.find(sig);
				if (it != ctx.nodes.end())
				{
					for (const auto& childSig : it->second.childSigs)
					{
						if (!childSig.empty())
						{
							effectiveCounts[childSig]++;
						}
					}
				}
			}

			// Propagate counts down through non-extracted nodes in reverse post-order (top-down)
			for (auto it = ctx.postOrder.rbegin(); it != ctx.postOrder.rend(); ++it)
			{
				const std::string& sig = *it;
				if (extracted.find(sig) == extracted.end())
				{
					int count = effectiveCounts[sig];
					if (count > 0)
					{
						auto nodeIt = ctx.nodes.find(sig);
						if (nodeIt != ctx.nodes.end())
						{
							for (const auto& childSig : nodeIt->second.childSigs)
							{
								if (!childSig.empty())
								{
									effectiveCounts[childSig] += count;
								}
							}
						}
					}
				}
			}
		}

		/**
		 * @brief Emits GLSL code with local variable declarations for extracted expressions in topological order.
		 */
		inline std::string emitWithCSE(const std::string& sig, const Context& ctx,
									   const std::unordered_set<std::string>& extracted,
									   std::unordered_map<std::string, std::string>& varMap,
									   std::unordered_set<std::string>& emitted, std::vector<std::string>& statements,
									   int& varCounter)
		{
			if (sig.empty())
				return "";

			auto nodeIt = ctx.nodes.find(sig);
			if (nodeIt == ctx.nodes.end() || nodeIt->second.expr->isTrivial())
			{
				return sig;
			}

			bool isExt = (extracted.find(sig) != extracted.end());

			// If already emitted as a local variable, reuse the variable name
			if (isExt && emitted.find(sig) != emitted.end())
			{
				return varMap[sig];
			}

			// Recursively emit children first (guarantees topological / dependency order)
			std::vector<std::string> childResults;
			childResults.reserve(nodeIt->second.childSigs.size());
			for (const auto& childSig : nodeIt->second.childSigs)
			{
				childResults.push_back(emitWithCSE(childSig, ctx, extracted, varMap, emitted, statements, varCounter));
			}

			std::string rhs = nodeIt->second.expr->printWithChildren(childResults);

			if (isExt)
			{
				std::string varName = "_v" + std::to_string(varCounter++);
				statements.push_back("\tfloat " + varName + " = " + rhs + ";\n");
				emitted.insert(sig);
				varMap[sig] = varName;
				return varName;
			}

			return rhs;
		}

		/**
		 * @brief Generates an optimized `evaluate_sdf_<id>` GLSL function from an AST root.
		 */
		inline std::string generateFunction(const std::shared_ptr<IMathExpression>& root, ShapeId id, bool is3D = false)
		{
			Context ctx;
			std::string rootSig = collectSubtrees(root, ctx);

			// Initial candidate set: all non-trivial nodes appearing >= 2 times
			std::unordered_set<std::string> extracted;
			for (const auto& [sig, info] : ctx.nodes)
			{
				if (!info.expr->isTrivial() && info.rawCount >= 2)
				{
					extracted.insert(sig);
				}
			}

			// Iteratively prune candidates whose effective count drops below 2
			std::unordered_map<std::string, int> effectiveCounts;
			bool changed = true;
			while (changed)
			{
				changed = false;
				computeEffectiveCounts(rootSig, ctx, extracted, effectiveCounts);
				for (auto it = extracted.begin(); it != extracted.end();)
				{
					if (effectiveCounts[*it] < 2)
					{
						it = extracted.erase(it);
						changed = true;
					}
					else
					{
						++it;
					}
				}
			}

			std::unordered_map<std::string, std::string> varMap;
			std::unordered_set<std::string> emitted;
			std::vector<std::string> statements;
			int varCounter = 0;

			std::string finalExpr = emitWithCSE(rootSig, ctx, extracted, varMap, emitted, statements, varCounter);

			std::ostringstream oss;
			if (is3D)
			{
				oss << "float evaluate_sdf_" << id << "(in vec3 p, in vec4 parameters0, in vec4 parameters1)\n{\n";
			}
			else
			{
				oss << "float evaluate_sdf_" << id << "(in vec2 p, in vec4 parameters0, in vec4 parameters1)\n{\n";
			}
			for (const auto& stmt : statements)
			{
				oss << stmt;
			}
			oss << "\treturn " << finalExpr << ";\n";
			oss << "}\n\n";

			return oss.str();
		}
	} // namespace CSE

	/**
	 * @brief Primary entry point for updating SDF shader code from ECS components.
	 *
	 * Inspects all active shapes of type `ShapeClass` (e.g. `CustomShape` or `UIShape`), sorts
	 * them by `groupIdx`, generates dedicated evaluation functions with CSE, and injects
	 * both helper functions (Slot 0) and the evaluation loop (Slot 1) into the target shader.
	 *
	 * @tparam ShapeClass ECS component type (`CustomShape` or `UIShape`).
	 * @tparam RenderContext Render pipeline context tracking update dirty flags.
	 * @param registry Reference to the active ECS registry.
	 * @param ctx Reference to render pipeline context.
	 * @param shader Target GLSL Shader program to recompile.
	 * @param sdfs Array of registered AST mathematical expressions indexed by `distanceFieldId`.
	 */
	template <typename ShapeClass, typename RenderContext, bool is3D = false>
	inline void update(Registry& registry, RenderContext& ctx, WeirdRenderer::Shader& shader,
					   const std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		const auto componentArray = registry.getComponentManager<ShapeClass>()->getComponentArray();

		if (!ctx.shapesNeedUpdate)
		{
			return;
		}

#if !defined(NDEBUG) && defined(LOG_SDF_SHADER_GENERATION)
		std::cout << "Updating shader code for: " << std::string(typeid(ShapeClass).name()) << "\n";
#endif

		ctx.shapesNeedUpdate = false;

		auto toGlslFloat = [](float value)
		{
			std::ostringstream ss;
			ss << std::fixed << std::setprecision(6) << value;
			return ss.str();
		};

		std::ostringstream oss;			 // Slot 1: injected into #include "custom_shapes"
		std::ostringstream functionsOss; // Slot 0: injected into #include "helper_functions"

		// =========================================================================================
		// 1. Sort Shapes by Group Index
		// =========================================================================================
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

		// =========================================================================================
		// 3. Emit Unique Shape Evaluation Functions with CSE
		// =========================================================================================
		std::unordered_set<ShapeId> emittedShapes;
		for (size_t idx = 0; idx < componentArray->getSize(); idx++)
		{
			size_t i = orderedIndices.at(idx);
			const ShapeClass& shape = componentArray->getDataAtIdx(i);
			ShapeId id = shape.distanceFieldId;
			if (id < sdfs.size() && sdfs[id] && emittedShapes.find(id) == emittedShapes.end())
			{
				emittedShapes.insert(id);
				functionsOss << CSE::generateFunction(sdfs[id], id, is3D);
			}
		}

		// =========================================================================================
		// 4. Emit Shape Dispatch Loop inside getDistanceMaterialMask / sceneSdf
		// =========================================================================================
		for (size_t idx = 0; idx < componentArray->getSize() + 1; idx++)
		{
			size_t i = idx == componentArray->getSize() ? 0 : orderedIndices.at(idx);
			const ShapeClass& shape = idx == componentArray->getSize() ? dummyShape : componentArray->getDataAtIdx(i);
			const int group = shape.groupIdx;

			// Handle Group Transitions and Group Flushes
			if (group != currentGroup)
			{
				if (currentGroup != -1)
				{
					oss << "flushShapeGroup(" << groupDistanceVariable << ", " << groupBlendVariable
						<< ", currentGroupColor, minDist, globalBlend, finalMaterialId);\n";
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

			bool globalEffect = group == CustomShape::GLOBAL_GROUP;
			std::string targetDist = globalEffect ? "minDist" : groupDistanceVariable;
			std::string targetBlend = globalEffect ? "globalBlend" : groupBlendVariable;
			std::string targetColor = globalEffect ? "finalMaterialId" : "currentGroupColor";

			// Per-Instance Evaluation & CSG Combination Block
			oss << "{\n";
			oss << "\tvec4 p0, p1;\n";
			oss << "\tfetchShapeParams(dataOffset + " << 2 * i << ", p0, p1);\n";
			if (emittedShapes.find(shape.distanceFieldId) == emittedShapes.end())
			{
				oss << "\tfloat dist = 10000.0;\n";
			}
			else if (is3D)
			{
				oss << "\tfloat dist = evaluate_sdf_" << shape.distanceFieldId << "(p, p0, p1);\n";
			}
			else
			{
				oss << "\tfloat dist = evaluate_sdf_" << shape.distanceFieldId << "(p.xy, p0, p1);\n";
			}
			oss << "\tdist = modifyDistanceBasedOnMaterial(dist, " << shape.material << ", dataOffset + " << 2 * i
				<< ");\n";

			switch (shape.combination)
			{
				case CombinationType::Addition:
				{
					oss << "\tapplyShapeAddition(dist, " << shape.material << ", " << targetDist << ", " << targetBlend
						<< ", " << targetColor << ");\n";
					break;
				}
				case CombinationType::Subtraction:
				{
					oss << "\tapplyShapeSubtraction(dist, " << targetDist << ");\n";
					break;
				}
				case CombinationType::Intersection:
				{
					oss << "\tapplyShapeIntersection(dist, " << targetDist << ");\n";
					break;
				}
				case CombinationType::SmoothAddition:
				{
					oss << "\tapplyShapeSmoothAddition(dist, " << shape.material << ", "
						<< toGlslFloat(shape.smoothFactor) << ", " << targetDist << ", " << targetBlend << ", "
						<< targetColor << ");\n";
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					oss << "\tapplyShapeSmoothSubtraction(dist, " << toGlslFloat(shape.smoothFactor) << ", "
						<< targetDist << ");\n";
					break;
				}
				default:
					break;
			}
			oss << "}\n\n";
		}

		std::string replacement = oss.str();
		std::string helperFunctionsTotal = functionsOss.str();

		// Inject generated GLSL blocks into the shader's include slots
		shader.setFragmentIncludeCode(0, helperFunctionsTotal, false);
		shader.setFragmentIncludeCode(1, replacement, true);

#if !defined(NDEBUG) && defined(LOG_SDF_SHADER_GENERATION)
		if (!helperFunctionsTotal.empty())
		{
			WeirdEngine::Logger::log("HELPER FUNCTIONS TOTAL:\n" + helperFunctionsTotal);
		}

		WeirdEngine::Logger::log(replacement);

		static int shaderDumpId = 0;
		std::ofstream outFile("generated_shader_" + std::string(typeid(ShapeClass).name()) + "_" +
							  std::to_string(shaderDumpId++) + ".frag");
		if (outFile.is_open())
		{
			outFile << shader.getFragmentCode();
			outFile.close();
		}
#endif
	}
} // namespace WeirdEngine::SDFShaderGenerationSystem