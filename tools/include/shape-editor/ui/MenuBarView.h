#pragma once

#include "shape-editor/model/NodeGraph.h"
#include "shape-editor/services/EditorFileManager.h"
#include <functional>
#include <imgui.h>
#include <weird-engine/services/ServiceProvider.h>

namespace WeirdEngine::Tools::ShapeEd
{
	class MenuBarView
	{
	public:
		static void render(ServiceProvider& services, NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
						   bool& outSyncPositionsToImNodes, bool& hasUnsavedChanges,
						   const std::function<void(std::function<void()>)>& confirmAction = nullptr);
		static void renderAddNodeMenu(NodeGraph& graph, ImVec2 spawnPos, bool& outDirty);
	};
} // namespace WeirdEngine::Tools::ShapeEd
