#include "MainMenuScene.h"

#include "ToolsSession.h"
#include "weird-renderer/core/Display.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <imgui.h>
#include <unordered_set>

namespace WeirdEngine::Tools
{
	namespace
	{
		std::mutex& liveMainMenusMutex()
		{
			static std::mutex m;
			return m;
		}

		std::unordered_set<MainMenuScene*>& liveMainMenus()
		{
			static std::unordered_set<MainMenuScene*> s;
			return s;
		}

		std::string toLowerCopy(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		const char* getRecentBadgeText(RecentItemType type)
		{
			switch (type)
			{
				case RecentItemType::Project:
					return "PROJECT";
				case RecentItemType::Scene:
					return "SCENE";
				case RecentItemType::Cluster:
					return "CLUSTER";
				case RecentItemType::Shape:
				default:
					return "SHAPE";
			}
		}

		ImVec4 getRecentBadgeColor(RecentItemType type)
		{
			switch (type)
			{
				case RecentItemType::Project:
					return ImVec4(0.55f, 0.45f, 0.85f, 1.0f);
				case RecentItemType::Scene:
					return ImVec4(0.24f, 0.56f, 0.90f, 1.0f);
				case RecentItemType::Cluster:
					return ImVec4(0.18f, 0.68f, 0.65f, 1.0f);
				case RecentItemType::Shape:
				default:
					return ImVec4(0.90f, 0.55f, 0.18f, 1.0f);
			}
		}
	} // namespace

	MainMenuScene::~MainMenuScene()
	{
		std::lock_guard<std::mutex> lock(liveMainMenusMutex());
		liveMainMenus().erase(this);
	}

	void MainMenuScene::onStart(Registry& /*registry*/, ServiceProvider& services)
	{
		{
			std::lock_guard<std::mutex> lock(liveMainMenusMutex());
			liveMainMenus().insert(this);
		}

		ToolsSession& session = ToolsSession::getInstance();
		session.loadConfig();
		session.clearEditorSessions();

		services.render().resetWorldSmoothFactor();
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		auto& bg = services.render().getBackground();
		bg.type = BackgroundType::Solid;
		bg.primaryColor = vec4(0.07f, 0.07f, 0.08f, 1.0f);
	}

	void MainMenuScene::onDestroy(Registry& /*registry*/, ServiceProvider& /*services*/)
	{
		std::lock_guard<std::mutex> lock(liveMainMenusMutex());
		liveMainMenus().erase(this);
	}

	void MainMenuScene::onUpdate(Registry& /*registry*/, ServiceProvider& services)
	{
		pollPendingDialogs(services);
	}

	void SDLCALL MainMenuScene::onDialogCallback(void* userdata, const char* const* filelist, int /*filter*/)
	{
		std::unique_ptr<DialogRequestData> req(static_cast<DialogRequestData*>(userdata));
		if (!req || !req->scene || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> liveLock(liveMainMenusMutex());
		if (liveMainMenus().count(req->scene) == 0)
		{
			return;
		}

		std::lock_guard<std::mutex> dialogLock(req->scene->m_dialogMutex);
		req->scene->m_pendingKind = req->kind;
		req->scene->m_pendingPath = *filelist;
	}

	void MainMenuScene::openProjectDialog()
	{
		std::string defaultDir = ToolsSession::getDefaultProjectBrowseDirectory();
		auto* req = new DialogRequestData{this, PendingDialogKind::ProjectFolder};
		ToolsSession::showOpenFolderDialog(onDialogCallback, req, defaultDir, "Select Weird Engine Project Folder");
	}

	void MainMenuScene::openClusterFileDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Cluster (*.weirdcluster)", "weirdcluster"}};
		const std::string& activeProject = ToolsSession::getInstance().getActiveProjectRoot();
		std::string defaultDir;
		if (!activeProject.empty() && std::filesystem::exists(std::filesystem::path(activeProject) / "assets"))
		{
			defaultDir = (std::filesystem::path(activeProject) / "assets").string();
		}
		auto* req = new DialogRequestData{this, PendingDialogKind::ClusterFile};
		SDL_ShowOpenFileDialog(onDialogCallback, req, nullptr, filters, 1,
							   defaultDir.empty() ? nullptr : defaultDir.c_str(), false);
	}

	void MainMenuScene::openShapeFileDialog()
	{
		static SDL_DialogFileFilter filters[1] = {{"Weird Shape (*.weirdshape)", "weirdshape"}};
		const std::string& activeProject = ToolsSession::getInstance().getActiveProjectRoot();
		std::string defaultDir;
		if (!activeProject.empty() && std::filesystem::exists(std::filesystem::path(activeProject) / "assets"))
		{
			defaultDir = (std::filesystem::path(activeProject) / "assets").string();
		}
		auto* req = new DialogRequestData{this, PendingDialogKind::ShapeFile};
		SDL_ShowOpenFileDialog(onDialogCallback, req, nullptr, filters, 1,
							   defaultDir.empty() ? nullptr : defaultDir.c_str(), false);
	}

	void MainMenuScene::pollPendingDialogs(ServiceProvider& services)
	{
		PendingDialogKind kind = PendingDialogKind::None;
		std::string path;
		{
			std::lock_guard<std::mutex> lock(m_dialogMutex);
			if (m_pendingKind != PendingDialogKind::None && !m_pendingPath.empty())
			{
				kind = m_pendingKind;
				path = std::move(m_pendingPath);
				m_pendingKind = PendingDialogKind::None;
				m_pendingPath.clear();
			}
		}

		if (kind == PendingDialogKind::None || path.empty())
		{
			return;
		}

		auto& session = ToolsSession::getInstance();
		switch (kind)
		{
			case PendingDialogKind::ProjectFolder:
				tryOpenProjectFolder(services, path);
				break;
			case PendingDialogKind::ClusterFile:
				session.openClusterInEditor(services, SCENE_MAIN_MENU, path, true);
				break;
			case PendingDialogKind::ShapeFile:
				session.openShapeInEditor(services, SCENE_MAIN_MENU, path, true);
				break;
			case PendingDialogKind::None:
			default:
				break;
		}
	}

	void MainMenuScene::onCustomUI(Registry& /*registry*/, ServiceProvider& services)
	{
		const int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
		const int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)), ImGuiCond_Always);

		const ImGuiWindowFlags rootFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
										   ImGuiWindowFlags_NoSavedSettings;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28.0f, 22.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.085f, 0.088f, 0.095f, 1.0f));

		if (ImGui::Begin("##WeirdEditorMainMenu", nullptr, rootFlags))
		{
			// Header row
			ImGui::SetWindowFontScale(1.45f);
			ImGui::TextColored(ImVec4(0.95f, 0.96f, 0.98f, 1.0f), "Weird Editor");
			ImGui::SetWindowFontScale(1.0f);

			renderInitProjectModal(services);

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			const float availW = ImGui::GetContentRegionAvail().x;
			const float colGap = 20.0f;
			const float leftW = std::floor((availW - colGap) * 0.46f);
			const float rightW = availW - leftW - colGap;

			renderToolsColumn(services, leftW);
			ImGui::SameLine(0.0f, colGap);
			renderRecentColumn(services, rightW);
		}
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);
	}

	void MainMenuScene::tryOpenProjectFolder(ServiceProvider& services, const std::string& path)
	{
		bool hasAssets = false;
		std::string resolved = ToolsSession::resolveProjectFolder(path, &hasAssets);
		if (resolved.empty())
		{
			return;
		}
		if (hasAssets)
		{
			ToolsSession::getInstance().openProjectInSceneEditor(services, SCENE_MAIN_MENU, resolved);
		}
		else
		{
			m_pendingNewProjectFolder = resolved;
			m_openInitProjectModal = true;
		}
	}

	void MainMenuScene::renderInitProjectModal(ServiceProvider& services)
	{
		if (m_openInitProjectModal)
		{
			ImGui::OpenPopup("Initialize Project Folder?");
			m_openInitProjectModal = false;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("Initialize Project Folder?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextWrapped("The selected folder does not contain an 'assets/' directory:");
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.45f, 0.78f, 1.0f, 1.0f), "%s", m_pendingNewProjectFolder.c_str());
			ImGui::Spacing();
			ImGui::TextWrapped("Create an 'assets/' subfolder there and open the project in Scene Editor?");
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::Button("Create & Open", ImVec2(140.0f, 0.0f)))
			{
				std::string target = std::move(m_pendingNewProjectFolder);
				m_pendingNewProjectFolder.clear();
				ImGui::CloseCurrentPopup();
				if (ToolsSession::getInstance().selectActiveProject(target, true))
				{
					ToolsSession::getInstance().openProjectInSceneEditor(services, SCENE_MAIN_MENU, target);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
			{
				m_pendingNewProjectFolder.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void MainMenuScene::renderToolsColumn(ServiceProvider& services, float colWidth)
	{
		auto& session = ToolsSession::getInstance();

		ImGui::BeginChild("##MainMenuToolsColumn", ImVec2(colWidth, 0.0f), false);
		ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.88f, 1.0f), "START");
		ImGui::Spacing();

		auto renderToolCard = [&](const char* childId, const char* title, const char* description, ImVec4 accentColor,
								  const char* primaryLabel, auto&& onPrimary, const char* secondaryLabel = nullptr,
								  auto&& onSecondary = nullptr)
		{
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.125f, 0.130f, 0.140f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.24f, 0.25f, 0.28f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

			if (ImGui::BeginChild(childId, ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY))
			{
				ImGui::SetWindowFontScale(1.15f);
				ImGui::TextColored(accentColor, "%s", title);
				ImGui::SetWindowFontScale(1.0f);

				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.77f, 0.80f, 1.0f));
				ImGui::TextWrapped("%s", description);
				ImGui::PopStyleColor();
				ImGui::Spacing();

				ImGui::PushStyleColor(
					ImGuiCol_Button, ImVec4(accentColor.x * 0.45f, accentColor.y * 0.45f, accentColor.z * 0.45f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(accentColor.x * 0.65f, accentColor.y * 0.65f,
																	 accentColor.z * 0.65f, 1.0f));
				if (ImGui::Button(primaryLabel, ImVec2(0.0f, 28.0f)))
				{
					onPrimary();
				}
				ImGui::PopStyleColor(2);

				if (secondaryLabel != nullptr)
				{
					ImGui::SameLine();
					if (ImGui::Button(secondaryLabel, ImVec2(0.0f, 28.0f)))
					{
						onSecondary();
					}
				}
			}
			ImGui::EndChild();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor(2);
			ImGui::Spacing();
		};

		// 1. Project (Scene Editor) Card
		renderToolCard(
			"##CardProject", "Project (Scene Editor)",
			"Open a project folder in the Scene Editor to browse scenes & assets, assemble 2D SDF worlds, and test "
			"physics.",
			ImVec4(0.32f, 0.66f, 0.98f, 1.0f), "Open Project...", [&]() { openProjectDialog(); }, nullptr, []() {});

		// 2. Cluster Editor Card
		renderToolCard(
			"##CardClusterEditor", "Cluster Editor",
			"Create or tweak individual rigid-body particle clusters (.weirdcluster) with springs and constraints.",
			ImVec4(0.24f, 0.80f, 0.74f, 1.0f), "New Cluster",
			[&]() { session.openNewClusterInEditor(services, SCENE_MAIN_MENU); }, "Open Cluster...",
			[&]() { openClusterFileDialog(); });

		// 3. Shape Editor Card
		renderToolCard(
			"##CardShapeEditor", "Shape Editor",
			"Create or tweak individual procedural SDF shapes (.weirdshape) with live preview and audio synthesis.",
			ImVec4(0.95f, 0.62f, 0.24f, 1.0f), "New Shape",
			[&]() { session.openNewShapeInEditor(services, SCENE_MAIN_MENU); }, "Open Shape...",
			[&]() { openShapeFileDialog(); });

		ImGui::EndChild();
	}

	void MainMenuScene::renderRecentColumn(ServiceProvider& services, float colWidth)
	{
		auto& session = ToolsSession::getInstance();

		ImGui::BeginChild("##MainMenuRecentColumn", ImVec2(colWidth, 0.0f), false);
		ImGui::TextColored(ImVec4(0.75f, 0.80f, 0.88f, 1.0f), "RECENT");
		ImGui::Spacing();

		// Filter buttons + Clear
		const char* filterNames[] = {"All", "Projects", "Clusters", "Shapes"};
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			const bool active = (m_recentFilter == i);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.24f, 0.50f, 0.80f, 1.0f));
			}
			if (ImGui::Button(filterNames[i]))
			{
				m_recentFilter = i;
			}
			if (active)
			{
				ImGui::PopStyleColor();
			}
		}

		ImGui::SameLine((std::max)(0.0f, ImGui::GetWindowWidth() - 62.0f));
		if (ImGui::Button("Clear", ImVec2(62.0f, 0.0f)))
		{
			if (m_recentFilter == 0)
			{
				session.clearRecent(RecentItemType::Project);
				session.clearRecent(RecentItemType::Cluster);
				session.clearRecent(RecentItemType::Shape);
			}
			else if (m_recentFilter == 1)
				session.clearRecent(RecentItemType::Project);
			else if (m_recentFilter == 2)
				session.clearRecent(RecentItemType::Cluster);
			else if (m_recentFilter == 3)
				session.clearRecent(RecentItemType::Shape);
		}

		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##RecentSearch", "Filter recent projects, clusters, and shapes...", m_recentSearchBuf,
								 sizeof(m_recentSearchBuf));
		ImGui::Spacing();

		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.115f, 0.120f, 0.130f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.23f, 0.26f, 1.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

		if (ImGui::BeginChild("##RecentItemsList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
		{
			const std::string searchLower = toLowerCopy(m_recentSearchBuf);
			const auto recentCopy = session.getRecentItems();

			int visibleCount = 0;
			std::optional<RecentItem> itemToOpen;
			std::optional<RecentItem> itemToRemove;

			for (size_t i = 0; i < recentCopy.size(); ++i)
			{
				const auto& item = recentCopy[i];

				if (item.type == RecentItemType::Scene)
					continue;
				if (m_recentFilter == 1 && item.type != RecentItemType::Project)
					continue;
				if (m_recentFilter == 2 && item.type != RecentItemType::Cluster)
					continue;
				if (m_recentFilter == 3 && item.type != RecentItemType::Shape)
					continue;

				std::filesystem::path p(item.path);
				std::string displayName = p.filename().string();
				if (displayName.empty())
				{
					displayName = item.path;
				}

				if (!searchLower.empty())
				{
					if (toLowerCopy(displayName).find(searchLower) == std::string::npos &&
						toLowerCopy(item.path).find(searchLower) == std::string::npos)
					{
						continue;
					}
				}

				++visibleCount;
				std::error_code ec;
				const bool existsOnDisk = std::filesystem::exists(p, ec);

				ImGui::PushID(static_cast<int>(i));

				const float rowHeight = 42.0f;
				const float removeBtnW = 26.0f;
				const float rowW = std::max(60.0f, ImGui::GetContentRegionAvail().x - removeBtnW - 8.0f);
				const ImVec2 rowStart = ImGui::GetCursorPos();

				if (!existsOnDisk)
				{
					ImGui::BeginDisabled();
				}

				if (ImGui::Selectable("##recent_row", false, ImGuiSelectableFlags_AllowOverlap,
									  ImVec2(rowW, rowHeight)))
				{
					itemToOpen = item;
				}

				if (!existsOnDisk)
				{
					ImGui::EndDisabled();
				}

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					ImGui::SetTooltip("%s%s", item.path.c_str(), existsOnDisk ? "" : " (Missing on disk)");
				}

				// Render row contents directly in the Selectable rect so long paths clip cleanly
				const ImVec2 rowMin = ImGui::GetItemRectMin();
				const ImVec2 rowMax = ImGui::GetItemRectMax();
				ImDrawList* drawList = ImGui::GetWindowDrawList();
				drawList->PushClipRect(rowMin, rowMax, true);

				char badgeBuf[24];
				std::snprintf(badgeBuf, sizeof(badgeBuf), "[%s]", getRecentBadgeText(item.type));
				const ImVec4 badgeCol = getRecentBadgeColor(item.type);
				drawList->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 4.0f), ImGui::ColorConvertFloat4ToU32(badgeCol),
								  badgeBuf);

				const float badgeW = ImGui::CalcTextSize(badgeBuf).x;
				const bool isOpenProject = (item.type == RecentItemType::Project && session.sceneState().hasState &&
											item.path == session.sceneState().projectRoot);
				std::string titleText = displayName;
				if (!existsOnDisk)
				{
					titleText += " (missing)";
				}
				else if (isOpenProject)
				{
					titleText += " (open)";
				}
				const ImU32 titleCol =
					existsOnDisk ? (isOpenProject ? IM_COL32(125, 205, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text))
								 : ImGui::GetColorU32(ImGuiCol_TextDisabled);
				drawList->AddText(ImVec2(rowMin.x + 14.0f + badgeW, rowMin.y + 4.0f), titleCol, titleText.c_str());

				drawList->AddText(ImVec2(rowMin.x + 8.0f, rowMin.y + 22.0f), IM_COL32(140, 145, 158, 255),
								  item.path.c_str());
				drawList->PopClipRect();

				ImGui::SameLine(0.0f, 6.0f);
				ImGui::SetCursorPosY(rowStart.y + 9.0f);
				if (ImGui::Button("x", ImVec2(removeBtnW, 24.0f)))
				{
					itemToRemove = item;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Remove from recent");
				}

				ImGui::SetCursorPos(ImVec2(rowStart.x, rowStart.y + rowHeight));
				ImGui::Dummy(ImVec2(0.0f, 4.0f));
				ImGui::PopID();
			}

			if (visibleCount == 0)
			{
				ImGui::Spacing();
				ImGui::TextDisabled("  No recent items matching filter.");
				ImGui::TextDisabled("  Open a project, cluster, or shape to see it here.");
			}

			if (itemToRemove.has_value())
			{
				session.removeRecentItem(itemToRemove->type, itemToRemove->path);
			}
			else if (itemToOpen.has_value())
			{
				switch (itemToOpen->type)
				{
					case RecentItemType::Project:
						tryOpenProjectFolder(services, itemToOpen->path);
						break;
					case RecentItemType::Cluster:
						session.openClusterInEditor(services, SCENE_MAIN_MENU, itemToOpen->path, true);
						break;
					case RecentItemType::Shape:
						session.openShapeInEditor(services, SCENE_MAIN_MENU, itemToOpen->path, true);
						break;
					case RecentItemType::Scene:
					default:
						break;
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(2);

		ImGui::EndChild();
	}
} // namespace WeirdEngine::Tools
