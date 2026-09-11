#pragma once

#include "model/NodeGraph.h"

namespace WeirdEngine::Editor
{
	class NodeGraphPresets
	{
	public:
		static void loadCircle(NodeGraph& graph);
		static void loadStar(NodeGraph& graph);
		static void loadAquaticWave(NodeGraph& graph);
		static void loadCsgRing(NodeGraph& graph);
		static void loadInfiniteRepeat(NodeGraph& graph);
	};
} // namespace WeirdEngine::Editor
