#pragma once

#include "scene-editor/core/EditorContext.h"

namespace WeirdEngine::Tools::SceneEd
{
	class SceneOperations;
	class ExternalToolsManager;

	class ContentBrowserView
	{
	public:
		void renderAssetBrowserTab(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
	};
} // namespace WeirdEngine::Tools::SceneEd
