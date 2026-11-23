#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "weird-engine/shapes/NodeGraph.h"

namespace WeirdEngine
{
	class NodeGraphCompiler
	{
	public:
		static Expr evaluate(NodeGraph& graph);
		static Expr evaluate(NodeGraph& graph, const Vec2Expr& samplePoint);
		static std::string generateCppCode(const NodeGraph& graph);

	private:
		static NodeValue evaluateNode(const NodeGraph& graph, int nodeId, int outputPinIdx,
									  std::unordered_set<int>& activeNodes, std::unordered_map<int, NodeValue>& cache,
									  const Vec2Expr* customPoint = nullptr);
		static std::string generateNodeCpp(const NodeGraph& graph, int nodeId, int outputPinIdx,
										   std::unordered_set<int>& activeNodes,
										   std::unordered_map<int, std::string>& cache,
										   const std::unordered_map<int, int>& pinUseCounts,
										   std::vector<std::string>& lines);
	};
} // namespace WeirdEngine

namespace WeirdEngine::Editor
{
	using namespace WeirdEngine;
} // namespace WeirdEngine::Editor
