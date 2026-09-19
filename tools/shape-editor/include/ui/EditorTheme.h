#pragma once

#include "model/NodeTypes.h"
#include <imgui.h>

namespace WeirdEngine::Editor
{
	class EditorTheme
	{
	public:
		static void setupImNodesStyle();
		static ImVec4 getCategoryColor(NodeCategory cat);
		static ImU32 getCategoryColorU32(NodeCategory cat);
	};
} // namespace WeirdEngine::Editor
