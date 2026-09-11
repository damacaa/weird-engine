#include "model/NodeGraphCompiler.h"

#include <sstream>

namespace WeirdEngine::Editor
{
	Expr NodeGraphCompiler::evaluate(NodeGraph& graph)
	{
		int outputId = graph.getOutputNodeId();
		if (outputId <= 0)
		{
			// Find first output node if not recorded
			for (const auto& [id, node] : graph.getNodes())
			{
				if (node.typeId == "sdf_output")
				{
					outputId = id;
					graph.setOutputNodeId(outputId);
					break;
				}
			}
		}

		if (outputId <= 0)
		{
			return Expr(0.0f);
		}

		std::unordered_set<int> visited;
		NodeValue val = evaluateNode(graph, outputId, visited);
		return asFloat(val);
	}

	std::string NodeGraphCompiler::generateCppCode(const NodeGraph& graph)
	{
		int outputId = graph.getOutputNodeId();
		if (outputId <= 0)
		{
			for (const auto& [id, node] : graph.getNodes())
			{
				if (node.typeId == "sdf_output")
				{
					outputId = id;
					break;
				}
			}
		}

		if (outputId <= 0)
			return "// No SDF Output node in graph";

		std::unordered_set<int> visited;
		std::vector<std::string> lines;
		std::string exprCode = generateNodeCpp(graph, outputId, visited, lines);

		std::ostringstream ss;
		ss << "// Generated Weird Engine SDF Expression\n";
		ss << "using namespace WeirdEngine;\n";
		ss << "using namespace WeirdEngine::SDF;\n\n";
		for (const auto& line : lines)
		{
			ss << line << "\n";
		}
		ss << "Expr finalShape = " << exprCode << ";\n";
		return ss.str();
	}

	NodeValue NodeGraphCompiler::evaluateNode(NodeGraph& graph, int nodeId, std::unordered_set<int>& visited)
	{
		// Cycle detection
		if (visited.count(nodeId))
		{
			return NodeValue(Expr(0.0f));
		}
		visited.insert(nodeId);

		const NodeInstance* node = graph.getNode(nodeId);
		if (!node)
			return NodeValue(Expr(0.0f));

		const NodeDef* def = NodeRegistry::get().findDef(node->typeId);
		if (!def)
			return NodeValue(Expr(0.0f));

		std::vector<NodeValue> evaluatedInputs;
		evaluatedInputs.reserve(def->inputs.size());

		for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
		{
			int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
			const LinkInstance* incomingLink = nullptr;
			for (const auto& link : graph.getLinks())
			{
				if (link.endPinId == pinId)
				{
					incomingLink = &link;
					break;
				}
			}

			if (incomingLink)
			{
				int upstreamNodeId, upstreamPinIdx;
				bool upstreamIsInput;
				decomposePinId(incomingLink->startPinId, upstreamNodeId, upstreamIsInput, upstreamPinIdx);

				evaluatedInputs.push_back(evaluateNode(graph, upstreamNodeId, visited));
			}
			else
			{
				// Use local default or overridden input value
				if (def->inputs[inIdx].type == PinType::Float)
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
		}

		visited.erase(nodeId);

		if (def->evaluate)
		{
			return def->evaluate(evaluatedInputs, *node);
		}

		return NodeValue(Expr(0.0f));
	}

	std::string NodeGraphCompiler::generateNodeCpp(const NodeGraph& graph, int nodeId, std::unordered_set<int>& visited,
												   std::vector<std::string>& lines)
	{
		if (visited.count(nodeId))
			return "/* cycle */";
		visited.insert(nodeId);

		const NodeInstance* node = graph.getNode(nodeId);
		if (!node)
			return "0.0f";

		const NodeDef* def = NodeRegistry::get().findDef(node->typeId);
		if (!def)
			return "0.0f";

		std::vector<std::string> inStrs;
		for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
		{
			int pinId = makePinId(nodeId, true, static_cast<int>(inIdx));
			const LinkInstance* incomingLink = nullptr;
			for (const auto& link : graph.getLinks())
			{
				if (link.endPinId == pinId)
				{
					incomingLink = &link;
					break;
				}
			}

			if (incomingLink)
			{
				int upNodeId, upPinIdx;
				bool upIsInput;
				decomposePinId(incomingLink->startPinId, upNodeId, upIsInput, upPinIdx);
				inStrs.push_back(generateNodeCpp(graph, upNodeId, visited, lines));
			}
			else
			{
				if (def->inputs[inIdx].type == PinType::Float)
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
		}

		visited.erase(nodeId);

		if (def->toCppCode)
		{
			return def->toCppCode(inStrs, *node);
		}
		return "0.0f";
	}
} // namespace WeirdEngine::Editor
