#include "scene-editor/SceneEditor.h"

#include "ToolsSession.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-renderer/components/Light2DComponent.h"

#include <chrono>
#include <filesystem>
#include <imgui.h>

namespace WeirdEngine
{
	using namespace Tools::SceneEd;

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

		services.render().setWorldSmoothFactor(0.5f);
		m_context.registry->registerComponent<Light2DComponent>();

		// Turn OFF debug input so clicking in the viewport does NOT spawn physics test balls!
		m_context.services->debug().setDebugInput(false);
		m_context.services->debug().setDebugFly(false);
		m_simulation2D.resume();
		m_simulation2D.setGravity(0.0f);
		m_simulation2D.setDamping(1.0f);

		services.render().getBackground().parallax = 1.0f;

		auto& cam = m_context.registry->getComponent<Transform>(m_context.services->render().getCameraEntity());
		cam.position = vec3(0.0f, 0.0f, 35.0f);

		// Initialize default materials if none exist
		for (size_t i = 0; i < 16; ++i)
		{
			auto& mat = m_context.services->materials2D().createMaterial("mat_" + std::to_string(i));
			if (i < ColorPalette::Default.size())
			{
				mat.color = WeirdEngine::vec4(ColorPalette::Default[i], 1.0f);
			}
		}

		// Load project configuration
		m_context.project.loadConfig();

		auto openDefaultOrActiveProject = [&]()
		{
			const std::string& activeProj = Tools::ToolsSession::getInstance().getActiveProjectRoot();
			if (!activeProj.empty() && std::filesystem::exists(activeProj))
			{
				m_context.project.open(activeProj);
			}
			else if (!m_context.project.getLastProject().empty() &&
					 std::filesystem::exists(m_context.project.getLastProject()))
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
		};

		auto syncResourceBasePath = [&]()
		{
			if (m_context.project.isOpen())
			{
				services.resources().setAssetsBasePath(m_context.project.getAssetsPath().generic_string() + "/");
			}
		};

		auto& session = Tools::ToolsSession::getInstance();
		Tools::ToolLaunchRequest launch = session.consumeSceneLaunchRequest();

		if (launch.mode == Tools::ToolLaunchRequest::Mode::OpenProject && !launch.path.empty())
		{
			m_context.project.open(launch.path);
			syncResourceBasePath();
		}
		else if (launch.mode == Tools::ToolLaunchRequest::Mode::OpenFile && !launch.path.empty())
		{
			std::string detectedProject = Tools::ToolsSession::detectProjectRootFromAssetPath(launch.path);
			if (!detectedProject.empty() && std::filesystem::exists(detectedProject))
			{
				m_context.project.open(detectedProject);
			}
			else
			{
				openDefaultOrActiveProject();
			}
			syncResourceBasePath();
			if (std::filesystem::exists(launch.path))
			{
				m_sceneOps.loadScene(launch.path);
			}
		}
		else if (launch.mode == Tools::ToolLaunchRequest::Mode::NewEmpty)
		{
			openDefaultOrActiveProject();
			syncResourceBasePath();
		}
		else if (session.sceneState().hasState)
		{
			restoreSessionState();
			syncResourceBasePath();
		}
		else
		{
			openDefaultOrActiveProject();
			syncResourceBasePath();
		}

		m_sceneOps.syncAllConstraintsClusterMembership();
		m_sceneOps.resetUndoBaseline(m_context.isSceneDirty);

		const Tools::PendingSceneExit pendingExit = session.consumePendingReturnExit();
		if (pendingExit == Tools::PendingSceneExit::MainMenu)
		{
			m_sceneOps.confirmIfDirty(
				[this]()
				{
					if (m_context.services)
						Tools::ToolsSession::getInstance().navigateToMainMenu(*m_context.services);
				});
		}
		else if (pendingExit == Tools::PendingSceneExit::Quit)
		{
			m_sceneOps.confirmIfDirty([]() { Tools::ToolsSession::requestApplicationQuit(); });
		}
	}

	void SceneEditor::onUpdate(Registry& registry, ServiceProvider& services)
	{
		m_context.scene = this;
		m_context.registry = &registry;
		m_context.services = &services;
		m_context.simulation2D = &m_simulation2D;
		m_context.worldRenderContext = &m_2DWorldRenderContext;
		m_context.uiRenderContext = &m_UIRenderContext;

		const std::string prevProjectRoot =
			m_context.project.isOpen() ? m_context.project.getRoot().string() : std::string();
		m_context.project.pollDialogs();
		if (m_context.project.isOpen() && m_context.project.getRoot().string() != prevProjectRoot)
		{
			services.resources().setAssetsBasePath(m_context.project.getAssetsPath().generic_string() + "/");
			if (!prevProjectRoot.empty())
			{
				m_sceneOps.newScene();
			}
		}
		m_sceneOps.pollFileActions();

		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_context.lastAssetPollTime).count() >= 250)
		{
			m_context.lastAssetPollTime = now;
			m_toolsMgr.pollAssetModifications();
		}

		m_viewport.handleHotkeys(m_context, m_sceneOps);
		m_viewport.handleCameraInput(m_context);
		const bool wasDragMoved = m_context.dragHasMoved;
		m_viewport.handleViewportMouseInput(m_context, m_sceneOps);
		m_sceneOps.noteViewportDragState(wasDragMoved || m_context.dragHasMoved);
	}

	void SceneEditor::onDestroy(Registry& registry, ServiceProvider& services)
	{
		m_context.scene = this;
		m_context.registry = &registry;
		m_context.services = &services;
		m_context.simulation2D = &m_simulation2D;
		m_context.worldRenderContext = &m_2DWorldRenderContext;
		m_context.uiRenderContext = &m_UIRenderContext;
		if (Tools::ToolsSession::getInstance().shouldPreserveSceneSessionOnExit())
		{
			saveSessionState();
		}
		else
		{
			Tools::ToolsSession::getInstance().clearSceneState();
		}
	}

	void SceneEditor::saveSessionState()
	{
		if (m_context.isSimulating)
		{
			m_sceneOps.toggleSimulation();
		}

		auto& state = Tools::ToolsSession::getInstance().sceneState();
		state.hasState = true;
		state.projectRoot = m_context.project.isOpen() ? m_context.project.getRoot().string() : std::string();
		state.currentScenePath = m_context.currentScenePath;
		state.isSceneDirty = m_context.isSceneDirty;
		state.gridEnabled = m_context.gridEnabled;
		state.gridSnap = m_context.gridSnap;
		state.gridSize = m_context.gridSize;
		state.playGravity = m_context.playGravity;
		state.playDamping = m_context.playDamping;
		state.selectIndividualClusterEntities = m_context.selectIndividualClusterEntities;
		state.clusterAssetWriteTimes = m_context.clusterAssetWriteTimes;

		Entity camEntity = m_context.services->render().getCameraEntity();
		if (m_context.registry->hasComponent<Transform>(camEntity))
		{
			state.cameraPosition = m_context.registry->getComponent<Transform>(camEntity).position;
		}

		// Map old entity IDs to the sequential IDs (1..N) assigned upon SceneSerializer::load
		std::unordered_map<Entity, uint32_t> entityRemap;
		uint32_t nextSeqId = 1;
		for (Entity e = 0; e < m_context.registry->getEntityCount(); ++e)
		{
			if (e == m_mainCamera || m_serializationBlacklist.count(e) > 0)
				continue;

			bool hasSerializedComp =
				m_context.registry->hasComponent<Transform>(e) || m_context.registry->hasComponent<Shape>(e) ||
				m_context.registry->hasComponent<UIShape>(e) || m_context.registry->hasComponent<Dot>(e) ||
				m_context.registry->hasComponent<RigidBody2D>(e) || m_context.registry->hasComponent<TextRenderer>(e);
			if (hasSerializedComp)
			{
				entityRemap[e] = nextSeqId++;
			}
		}

		state.savedGroups.clear();
		for (const auto& g : m_context.groups)
		{
			Tools::SavedEditorGroup sg;
			sg.id = g.id;
			sg.name = g.name;
			sg.clusterIds = g.clusterIds;
			for (Entity e : g.entities)
			{
				auto it = entityRemap.find(e);
				if (it != entityRemap.end())
				{
					sg.entityIds.push_back(it->second);
				}
			}
			state.savedGroups.push_back(std::move(sg));
		}

		if (state.tempSessionFilePath.empty())
		{
			state.tempSessionFilePath = Tools::ToolsSession::getSceneTempSessionPath();
		}
		SceneSerializer::save(*this, state.tempSessionFilePath);
	}

	void SceneEditor::restoreSessionState()
	{
		const auto& state = Tools::ToolsSession::getInstance().sceneState();
		if (!state.projectRoot.empty() && std::filesystem::exists(state.projectRoot))
		{
			m_context.project.open(state.projectRoot);
		}
		if (m_context.project.isOpen())
		{
			m_context.services->resources().setAssetsBasePath(m_context.project.getAssetsPath().generic_string() + "/");
		}

		if (!state.tempSessionFilePath.empty() && std::filesystem::exists(state.tempSessionFilePath))
		{
			m_sceneOps.newScene();
			SceneSerializer::load(*this, state.tempSessionFilePath);
			m_sceneOps.syncAllConstraintsClusterMembership();
		}

		m_context.currentScenePath = state.currentScenePath;
		m_context.isSceneDirty = state.isSceneDirty;
		m_context.gridEnabled = state.gridEnabled;
		m_context.gridSnap = state.gridSnap;
		m_context.gridSize = state.gridSize;
		m_context.playGravity = state.playGravity;
		m_context.playDamping = state.playDamping;
		m_context.selectIndividualClusterEntities = state.selectIndividualClusterEntities;
		m_context.clusterAssetWriteTimes = state.clusterAssetWriteTimes;

		Entity camEntity = m_context.services->render().getCameraEntity();
		if (m_context.registry->hasComponent<Transform>(camEntity))
		{
			auto& cam = m_context.registry->getComponent<Transform>(camEntity);
			cam.position = state.cameraPosition;
			m_context.registry->setComponentDirty(cam);
		}

		m_context.groups.clear();
		m_context.nextGroupId = 1;
		for (const auto& sg : state.savedGroups)
		{
			EditorGroup g;
			g.id = sg.id;
			g.name = sg.name;
			g.clusterIds = sg.clusterIds;
			for (uint32_t eid : sg.entityIds)
			{
				Entity e = static_cast<Entity>(eid);
				if (m_context.registry->isEntityAlive(e))
				{
					g.entities.push_back(e);
				}
			}
			m_context.nextGroupId = std::max(m_context.nextGroupId, g.id + 1);
			m_context.groups.push_back(std::move(g));
		}

		// Auto-reload any cluster assets modified while editing in Cluster Editor
		const bool wasDirty = m_context.isSceneDirty;
		m_toolsMgr.pollAssetModifications();
		m_context.isSceneDirty = wasDirty;
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
		m_viewport.renderProjectModal(m_context, m_sceneOps);
		m_viewport.renderViewportContextMenu(m_context, m_sceneOps);
		m_viewport.handleCustomUIShortcuts(m_context, m_sceneOps);
		m_sceneOps.syncUndoStateAfterUI();
		m_sceneOps.renderUnsavedChangesModal();
	}

	void SceneEditor::renderLeftPanel()
	{
		const float winH = ImGui::GetIO().DisplaySize.y;
		const float panelH = winH - m_context.menuBarHeight;

		ImGui::SetNextWindowPos(ImVec2(0.0f, m_context.menuBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(EditorContext::LEFT_PANEL_WIDTH, panelH), ImGuiCond_Always);

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
