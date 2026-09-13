#pragma once

#include "model/NodeGraph.h"
#include "services/EditorFileManager.h"

namespace WeirdEngine::Editor
{
	class NodeCanvasView
	{
	public:
		static void render(NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
						   bool& syncPositionsToImNodes);
	};
} // namespace WeirdEngine::Editor
