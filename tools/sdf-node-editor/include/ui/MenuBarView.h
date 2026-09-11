#pragma once

#include "model/NodeGraph.h"
#include "services/EditorFileManager.h"
#include <imgui.h>

namespace WeirdEngine::Editor
{
	class MenuBarView
	{
	public:
		static void render(NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
						   bool& outSyncPositionsToImNodes);
		static void renderAddNodeMenu(NodeGraph& graph, ImVec2 spawnPos, bool& outDirty);
	};
} // namespace WeirdEngine::Editor
