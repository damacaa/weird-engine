#pragma once

#include "core/EditorContext.h"
#include "services/ExternalToolsManager.h"
#include "services/SceneOperations.h"
#include "ui/ContentBrowserView.h"
#include "ui/HierarchyView.h"
#include "ui/InspectorView.h"
#include "ui/MaterialEditorView.h"
#include "ui/MenuBarView.h"
#include "ui/ViewportInteraction.h"
#include <weird-engine.h>

extern WeirdEngine::vec3 g_cameraPositon;

namespace WeirdEngine
{
	class SceneEditor : public Scene2D
	{
	public:
		SceneEditor();
		~SceneEditor() override = default;

		void onStart(Registry& registry, ServiceProvider& services) override;
		void onUpdate(Registry& registry, ServiceProvider& services) override;
		void onCustomUI(Registry& registry, ServiceProvider& services) override;

	private:
		void renderLeftPanel();

		Editor::EditorContext m_context;
		Editor::SceneOperations m_sceneOps;
		Editor::ExternalToolsManager m_toolsMgr;
		Editor::ViewportInteraction m_viewport;
		Editor::MenuBarView m_menuBar;
		Editor::ContentBrowserView m_contentBrowser;
		Editor::HierarchyView m_hierarchy;
		Editor::MaterialEditorView m_materials;
		Editor::InspectorView m_inspector;
	};
} // namespace WeirdEngine
