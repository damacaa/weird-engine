#pragma once

#include "core/EditorContext.h"

namespace WeirdEngine::Editor
{
	class SceneOperations;
	class ExternalToolsManager;
	class ViewportInteraction;

	class MenuBarView
	{
	public:
		void renderMenuBar(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr,
						   ViewportInteraction& viewport);
	};
} // namespace WeirdEngine::Editor
