#pragma once

#include "shape-editor/model/NodeGraph.h"
#include "shape-editor/services/EditorFileManager.h"
#include <functional>

namespace WeirdEngine::Tools::ShapeEd
{
	class NodeCanvasView
	{
	public:
		static void render(NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
						   bool& syncPositionsToImNodes, bool& hasUnsavedChanges,
						   const std::function<void(std::function<void()>)>& confirmAction = nullptr);
	};
} // namespace WeirdEngine::Tools::ShapeEd
