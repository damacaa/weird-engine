#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "model/NodeGraph.h"

namespace WeirdEngine::Editor
{
	class NodeGraphCompiler
	{
	public:
		static Expr evaluate(NodeGraph& graph);
		static std::string generateCppCode(const NodeGraph& graph);

	private:
		static NodeValue evaluateNode(NodeGraph& graph, int nodeId, std::unordered_set<int>& visited);
		static std::string generateNodeCpp(const NodeGraph& graph, int nodeId, std::unordered_set<int>& visited,
										   std::vector<std::string>& lines);
	};
} // namespace WeirdEngine::Editor
