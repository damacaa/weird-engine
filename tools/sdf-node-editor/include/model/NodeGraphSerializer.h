#pragma once

#include "model/NodeGraph.h"
#include <json/json.h>

namespace WeirdEngine::Editor
{
	using json = nlohmann::json;

	class NodeGraphSerializer
	{
	public:
		static json serialize(const NodeGraph& graph);
		static bool deserialize(NodeGraph& graph, const json& j);
	};
} // namespace WeirdEngine::Editor
