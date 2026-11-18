#pragma once

#include "shape-editor/model/NodeTypes.h"
#include <imgui.h>

namespace WeirdEngine::Tools::ShapeEd
{
	class EditorTheme
	{
	public:
		static void setupImNodesStyle();
		static ImVec4 getCategoryColor(NodeCategory cat);
		static ImU32 getCategoryColorU32(NodeCategory cat);
	};
} // namespace WeirdEngine::Tools::ShapeEd
