#pragma once

#include "scene-editor/core/EditorContext.h"
#include "scene-editor/services/ExternalToolsManager.h"
#include "scene-editor/services/SceneOperations.h"
#include "scene-editor/ui/ContentBrowserView.h"
#include "scene-editor/ui/HierarchyView.h"
#include "scene-editor/ui/InspectorView.h"
#include "scene-editor/ui/MaterialEditorView.h"
#include "scene-editor/ui/MenuBarView.h"
#include "scene-editor/ui/ViewportInteraction.h"
#include <weird-engine.h>

namespace WeirdEngine
{
	class SceneEditor : public Scene2D
	{
	public:
		SceneEditor();
		~SceneEditor() override = default;

		void onStart(Registry& registry, ServiceProvider& services) override;
		void onUpdate(Registry& registry, ServiceProvider& services) override;
		void onDestroy(Registry& registry, ServiceProvider& services) override;
		void onCustomUI(Registry& registry, ServiceProvider& services) override;

	private:
		void renderLeftPanel();
		void saveSessionState();
		void restoreSessionState();

		Tools::SceneEd::EditorContext m_context;
		Tools::SceneEd::SceneOperations m_sceneOps;
		Tools::SceneEd::ExternalToolsManager m_toolsMgr;
		Tools::SceneEd::ViewportInteraction m_viewport;
		Tools::SceneEd::MenuBarView m_menuBar;
		Tools::SceneEd::ContentBrowserView m_contentBrowser;
		Tools::SceneEd::HierarchyView m_hierarchy;
		Tools::SceneEd::MaterialEditorView m_materials;
		Tools::SceneEd::InspectorView m_inspector;
	};
} // namespace WeirdEngine
