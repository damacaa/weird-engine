#pragma once

#include "scene-editor/core/EditorContext.h"

namespace WeirdEngine::Tools::SceneEd
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
} // namespace WeirdEngine::Tools::SceneEd
