#include "weird-engine/shapes/NodeGraphCompiler.h"

#include <sstream>

namespace WeirdEngine
{
	namespace
	{
		int findOutputNodeId(const NodeGraph& graph)
		{
			if (graph.getOutputNodeId() > 0)
			{
				return graph.getOutputNodeId();
			}
			for (const auto& [id, node] : graph.getNodes())
			{
				if (node.typeId == "sdf_output")
				{
					return id;
				}
			}
			return -1;
		}

		const LinkInstance* findIncomingLink(const NodeGraph& graph, int inputPinId)
		{
			for (const auto& link : graph.getLinks())
			{
				if (link.endPinId == inputPinId)
				{
					return &link;
				}
			}
			return nullptr;
		}

		std::string makeCppVarName(int nodeId, int outputPinIdx, size_t outputCount)
		{
			std::string name = "node" + std::to_string(nodeId);
			if (outputCount > 1)
			{
				name += "_" + std::to_string(outputPinIdx);
			}
			return name;
		}
	} // namespace

	Expr NodeGraphCompiler::evaluate(NodeGraph& graph)
	{
		int outputId = findOutputNodeId(graph);
		if (outputId <= 0)
		{
			return Expr(1.0f);
		}
		if (graph.getOutputNodeId() <= 0)
		{
			graph.setOutputNodeId(outputId);
		}

		std::unordered_set<int> activeNodes;
		std::unordered_map<int, NodeValue> cache;
		NodeValue val = evaluateNode(graph, outputId, 0, activeNodes, cache, nullptr);
		return asFloat(val);
	}

	Expr NodeGraphCompiler::evaluate(NodeGraph& graph, const Vec2Expr& samplePoint)
	{
		int outputId = findOutputNodeId(graph);
		if (outputId <= 0)
		{
			return Expr(1.0f);
		}
		if (graph.getOutputNodeId() <= 0)
		{
			graph.setOutputNodeId(outputId);
		}

		std::unordered_set<int> activeNodes;
		std::unordered_map<int, NodeValue> cache;
		NodeValue val = evaluateNode(graph, outputId, 0, activeNodes, cache, &samplePoint);
		return asFloat(val);
	}

	std::string NodeGraphCompiler::generateCppCode(const NodeGraph& graph)
	{
		int outputId = findOutputNodeId(graph);
		if (outputId <= 0)
			return "// No SDF Output node in graph";

		std::unordered_map<int, int> pinUseCounts;
		for (const auto& link : graph.getLinks())
		{
			pinUseCounts[link.startPinId]++;
		}

		std::unordered_set<int> activeNodes;
		std::unordered_map<int, std::string> cache;
		std::vector<std::string> lines;
		std::string exprCode = generateNodeCpp(graph, outputId, 0, activeNodes, cache, pinUseCounts, lines);

		std::ostringstream ss;

		for (size_t i = 0; i < graph.getParameterNames().size(); ++i)
		{
			const std::string& name = graph.getParameterName(i);
			if (!name.empty())
			{
				ss << "// var(" << i << "): " << name << "\n";
			}
		}

		ss << "\n";

		for (const auto& line : lines)
		{
			ss << line << "\n";
		}

		ss << "Expr finalShape = " << exprCode << ";\n";
		return ss.str();
	}

	NodeValue NodeGraphCompiler::evaluateNode(const NodeGraph& graph, int nodeId, int outputPinIdx,
											  std::unordered_set<int>& activeNodes,
											  std::unordered_map<int, NodeValue>& cache, const Vec2Expr* customPoint)
	{
		int cacheKey = makePinId(nodeId, false, outputPinIdx);
		if (auto it = cache.find(cacheKey); it != cache.end())
		{
			return it->second;
		}

		// Cycle guard (do not cache cycle fallback)
		if (!activeNodes.insert(nodeId).second)
		{
			return NodeValue(Expr(0.0f));
		}

		const NodeInstance* node = graph.getNode(nodeId);
		const NodeDef* def = node ? NodeRegistry::get().findDef(node->typeId) : nullptr;
		if (!node || !def)
		{
			activeNodes.erase(nodeId);
			return NodeValue(Expr(0.0f));
		}

		if (customPoint != nullptr && def->typeId == "point")
		{
			activeNodes.erase(nodeId);
			NodeValue result(*customPoint);
			cache[cacheKey] = result;
			return result;
		}

		std::vector<NodeValue> evaluatedInputs;
		evaluatedInputs.reserve(def->inputs.size());

		for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
		{
			int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
			if (const LinkInstance* incomingLink = findIncomingLink(graph, pinId))
			{
				int upstreamNodeId = 0;
				int upstreamPinIdx = 0;
				bool upstreamIsInput = false;
				decomposePinId(incomingLink->startPinId, upstreamNodeId, upstreamIsInput, upstreamPinIdx);

				evaluatedInputs.push_back(
					evaluateNode(graph, upstreamNodeId, upstreamPinIdx, activeNodes, cache, customPoint));
			}
			else if (node->typeId == "sdf_output")
			{
				evaluatedInputs.push_back(NodeValue(Expr(1.0f)));
			}
			else if (def->inputs[inIdx].type == PinType::Float)
			{
				float v =
					(inIdx < node->inputFloats.size()) ? node->inputFloats[inIdx] : def->inputs[inIdx].defaultFloat;
				evaluatedInputs.push_back(NodeValue(Expr(v)));
			}
			else
			{
				glm::vec2 v =
					(inIdx < node->inputVec2s.size()) ? node->inputVec2s[inIdx] : def->inputs[inIdx].defaultVec2;
				evaluatedInputs.push_back(NodeValue(Vec2Expr(v.x, v.y)));
			}
		}

		activeNodes.erase(nodeId);

		NodeValue result = NodeValue(Expr(0.0f));
		if (def->evaluatePin)
		{
			result = def->evaluatePin(evaluatedInputs, *node, outputPinIdx);
		}
		else if (def->evaluate)
		{
			result = def->evaluate(evaluatedInputs, *node);
		}

		cache.emplace(cacheKey, result);
		return result;
	}

	std::string NodeGraphCompiler::generateNodeCpp(const NodeGraph& graph, int nodeId, int outputPinIdx,
												   std::unordered_set<int>& activeNodes,
												   std::unordered_map<int, std::string>& cache,
												   const std::unordered_map<int, int>& pinUseCounts,
												   std::vector<std::string>& lines)
	{
		int cacheKey = makePinId(nodeId, false, outputPinIdx);
		if (auto it = cache.find(cacheKey); it != cache.end())
		{
			return it->second;
		}

		if (!activeNodes.insert(nodeId).second)
		{
			return "/* cycle */";
		}

		const NodeInstance* node = graph.getNode(nodeId);
		const NodeDef* def = node ? NodeRegistry::get().findDef(node->typeId) : nullptr;
		if (!node || !def)
		{
			activeNodes.erase(nodeId);
			return "0.0f";
		}

		std::vector<std::string> inStrs;
		inStrs.reserve(def->inputs.size());
		for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
		{
			int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
			if (const LinkInstance* incomingLink = findIncomingLink(graph, pinId))
			{
				int upNodeId = 0;
				int upPinIdx = 0;
				bool upIsInput = false;
				decomposePinId(incomingLink->startPinId, upNodeId, upIsInput, upPinIdx);
				inStrs.push_back(generateNodeCpp(graph, upNodeId, upPinIdx, activeNodes, cache, pinUseCounts, lines));
			}
			else if (node->typeId == "sdf_output")
			{
				inStrs.push_back("1.0f");
			}
			else if (def->inputs[inIdx].type == PinType::Float)
			{
				float v =
					(inIdx < node->inputFloats.size()) ? node->inputFloats[inIdx] : def->inputs[inIdx].defaultFloat;
				inStrs.push_back(std::to_string(v) + "f");
			}
			else
			{
				glm::vec2 v =
					(inIdx < node->inputVec2s.size()) ? node->inputVec2s[inIdx] : def->inputs[inIdx].defaultVec2;
				inStrs.push_back("Vec2Expr(" + std::to_string(v.x) + "f, " + std::to_string(v.y) + "f)");
			}
		}

		activeNodes.erase(nodeId);

		std::string exprCode = "0.0f";
		if (def->toCppCodePin)
		{
			exprCode = def->toCppCodePin(inStrs, *node, outputPinIdx);
		}
		else if (def->toCppCode)
		{
			exprCode = def->toCppCode(inStrs, *node);
		}

		auto useIt = pinUseCounts.find(cacheKey);
		const int useCount = (useIt != pinUseCounts.end()) ? useIt->second : 0;
		const bool shouldHoist = useCount > 1 && !def->inputs.empty() && node->typeId != "sdf_output";
		if (shouldHoist)
		{
			std::string varName = makeCppVarName(nodeId, outputPinIdx, def->outputs.size());
			lines.push_back("auto " + varName + " = " + exprCode + ";");
			exprCode = std::move(varName);
		}

		cache.emplace(cacheKey, exprCode);
		return exprCode;
	}
} // namespace WeirdEngine
