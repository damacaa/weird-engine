#pragma once

#include "weird-engine/shapes/NodeGraph.h"

namespace WeirdEngine
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
} // namespace WeirdEngine

namespace WeirdEngine::Editor
{
	using namespace WeirdEngine;
} // namespace WeirdEngine::Editor
