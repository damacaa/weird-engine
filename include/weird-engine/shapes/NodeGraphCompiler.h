#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "weird-engine/shapes/NodeGraph.h"

namespace WeirdEngine
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
} // namespace WeirdEngine

namespace WeirdEngine::Editor
{
	using namespace WeirdEngine;
} // namespace WeirdEngine::Editor
