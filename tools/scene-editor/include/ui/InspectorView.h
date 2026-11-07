#pragma once

#include "core/EditorContext.h"

namespace WeirdEngine::Editor
{
	class SceneOperations;
	class ExternalToolsManager;

	class InspectorView
	{
	public:
		void renderInspector(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);

	private:
		void renderGroupInspector(EditorContext& ctx, SceneOperations& sceneOps);
		void renderMultiSelectionInspector(EditorContext& ctx, SceneOperations& sceneOps);
		void renderClusterInspector(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
		void renderEntityInspector(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr);
	};
} // namespace WeirdEngine::Editor
