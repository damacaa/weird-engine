#pragma once

#include "core/EditorContext.h"

namespace WeirdEngine::Editor
{
	class SceneOperations;
	class ExternalToolsManager;

	class ContentBrowserView
	{
	public:
		void renderAssetBrowserTab(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
	};
} // namespace WeirdEngine::Editor
