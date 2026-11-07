#include "SceneEditor.h"

#include "weird-engine/components/Transform.h"
#include "weird-renderer/components/Light2DComponent.h"

#include <chrono>
#include <filesystem>
#include <imgui.h>

namespace WeirdEngine
{
	SceneEditor::SceneEditor()
		: m_sceneOps(m_context)
		, m_toolsMgr(m_context, m_sceneOps)
	{
	}

	void SceneEditor::onStart(Registry& registry, ServiceProvider& services)
	{
		m_context.scene = this;
		m_context.registry = &registry;
		m_context.services = &services;
		m_context.simulation2D = &m_simulation2D;
		m_context.worldRenderContext = &m_2DWorldRenderContext;
		m_context.uiRenderContext = &m_UIRenderContext;

		m_context.registry->registerComponent<Light2DComponent>();

		// Turn OFF debug input so clicking in the viewport does NOT spawn physics test balls!
		m_context.services->debug().setDebugInput(false);
		m_context.services->debug().setDebugFly(false);
		m_simulation2D.resume();
		m_simulation2D.setGravity(0.0f);
		m_simulation2D.setDamping(1.0f);

		services.render().getBackground().parallax = 1.0f;

		auto& cam = m_context.registry->getComponent<Transform>(m_context.services->render().getCameraEntity());
		cam.position = g_cameraPositon;

		// Initialize default materials if none exist
		for (size_t i = 0; i < 16; ++i)
		{
			auto& mat = m_context.services->materials2D().createMaterial("mat_" + std::to_string(i));
			if (i < ColorPalette::Default.size())
			{
				mat.color = vec4(ColorPalette::Default[i], 1.0f);
			}
		}

		// Load project configuration
		m_context.project.loadConfig();

		// Auto-open last project or fallback to examples/sample-scenes
		if (!m_context.project.getLastProject().empty() && std::filesystem::exists(m_context.project.getLastProject()))
		{
			m_context.project.open(m_context.project.getLastProject());
		}
		else if (std::filesystem::exists("examples/sample-scenes/assets"))
		{
			m_context.project.open("examples/sample-scenes");
		}
		else if (std::filesystem::exists("../examples/sample-scenes/assets"))
		{
			m_context.project.open("../examples/sample-scenes");
		}
		else if (std::filesystem::exists("../../examples/sample-scenes/assets"))
		{
			m_context.project.open("../../examples/sample-scenes");
		}

		m_sceneOps.syncAllConstraintsClusterMembership();
	}

	void SceneEditor::onUpdate(Registry& registry, ServiceProvider& services)
	{
		m_context.scene = this;
		m_context.registry = &registry;
		m_context.services = &services;
		m_context.simulation2D = &m_simulation2D;
		m_context.worldRenderContext = &m_2DWorldRenderContext;
		m_context.uiRenderContext = &m_UIRenderContext;

		auto& cam = m_context.registry->getComponent<Transform>(m_context.services->render().getCameraEntity());
		g_cameraPositon = cam.position;

		m_context.project.pollDialogs();
		m_sceneOps.pollFileActions();

		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_context.lastAssetPollTime).count() >= 250)
		{
			m_context.lastAssetPollTime = now;
			m_toolsMgr.pollAssetModifications();
		}

		m_viewport.handleHotkeys(m_context, m_sceneOps);
		m_viewport.handleCameraInput(m_context);
		m_viewport.handleViewportMouseInput(m_context, m_sceneOps);
	}

	void SceneEditor::onCustomUI(Registry& registry, ServiceProvider& services)
	{
		m_context.scene = this;
		m_context.registry = &registry;
		m_context.services = &services;
		m_context.simulation2D = &m_simulation2D;
		m_context.worldRenderContext = &m_2DWorldRenderContext;
		m_context.uiRenderContext = &m_UIRenderContext;

		m_menuBar.renderMenuBar(m_context, m_sceneOps, m_toolsMgr, m_viewport);
		renderLeftPanel();
		m_inspector.renderInspector(m_context, m_sceneOps, m_toolsMgr);
		m_viewport.renderBottomToolbar(m_context, m_sceneOps);
		m_viewport.renderProjectModal(m_context);
		m_viewport.renderViewportContextMenu(m_context, m_sceneOps);
	}

	void SceneEditor::renderLeftPanel()
	{
		const float winH = ImGui::GetIO().DisplaySize.y;
		const float panelH = winH - m_context.menuBarHeight;

		ImGui::SetNextWindowPos(ImVec2(0.0f, m_context.menuBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(Editor::EditorContext::LEFT_PANEL_WIDTH, panelH), ImGuiCond_Always);

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
									   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
									   ImGuiWindowFlags_NoCollapse;

		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		if (ImGui::Begin("##LeftPanel", nullptr, flags))
		{
			if (ImGui::BeginTabBar("##LeftTabs"))
			{
				if (ImGui::BeginTabItem("Assets"))
				{
					m_contentBrowser.renderAssetBrowserTab(m_context, m_sceneOps, m_toolsMgr);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Hierarchy"))
				{
					m_hierarchy.renderHierarchyTab(m_context, m_sceneOps, m_toolsMgr);
					ImGui::EndTabItem();
				}
				ImGuiTabItemFlags matTabFlags = 0;
				if (m_context.selectMaterialsTab)
				{
					matTabFlags |= ImGuiTabItemFlags_SetSelected;
					m_context.selectMaterialsTab = false;
				}
				if (ImGui::BeginTabItem("Materials", nullptr, matTabFlags))
				{
					m_materials.renderMaterialsTab(m_context);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}
} // namespace WeirdEngine
