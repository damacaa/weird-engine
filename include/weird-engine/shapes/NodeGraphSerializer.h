#pragma once

#include "weird-engine/shapes/NodeGraph.h"
#include <json/json.h>

namespace WeirdEngine
{
	using json = nlohmann::json;

	class NodeGraphSerializer
	{
	public:
		static json serialize(const NodeGraph& graph);
		static bool deserialize(NodeGraph& graph, const json& j);
	};
} // namespace WeirdEngine

namespace WeirdEngine::Editor
{
	using namespace WeirdEngine;
} // namespace WeirdEngine::Editor
