#include "scene-editor/ui/MenuBarView.h"
#include "scene-editor/services/ExternalToolsManager.h"
#include "scene-editor/services/SceneOperations.h"
#include "scene-editor/ui/ViewportInteraction.h"
#include "ToolsSession.h"

#include "weird-engine.h"

#include <algorithm>
#include <filesystem>
#include <imgui.h>

namespace WeirdEngine::Tools::SceneEd
{
	void MenuBarView::renderMenuBar(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr,
									ViewportInteraction& viewport)
	{
		(void)toolsMgr;
		const auto confirmLeave = [&sceneOps](std::function<void()> proceed)
		{ sceneOps.confirmIfDirty(std::move(proceed)); };

		if (ImGui::BeginMainMenuBar())
		{
			ctx.menuBarHeight = ImGui::GetWindowSize().y;

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Project Folder..."))
				{
					sceneOps.confirmIfDirty([&ctx]() { ctx.project.openFolderDialog(); });
				}

				if (ImGui::BeginMenu("Recent Projects"))
				{
					const auto recentProjects = ctx.project.getRecentProjects();
					if (recentProjects.empty())
					{
						ImGui::MenuItem("No Recent Projects", nullptr, false, false);
					}
					else
					{
						const std::string activeRoot =
							ctx.project.isOpen() ? ToolsSession::normalizePath(ctx.project.getRoot().string()) : "";
						for (size_t i = 0; i < recentProjects.size(); ++i)
						{
							const std::string& path = recentProjects[i];
							std::error_code ec;
							const bool exists = std::filesystem::exists(path, ec);
							std::string name = std::filesystem::path(path).filename().string();
							if (name.empty())
							{
								name = path;
							}
							const bool isActive =
								!activeRoot.empty() && ToolsSession::normalizePath(path) == activeRoot;
							std::string label = name + "##RecentProj" + std::to_string(i);
							if (ImGui::MenuItem(label.c_str(), nullptr, isActive, exists))
							{
								if (!isActive)
								{
									sceneOps.confirmIfDirty(
										[&ctx, &sceneOps, path]()
										{
											if (ctx.project.open(path))
											{
												sceneOps.newScene();
											}
										});
								}
							}
							if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
							{
								ImGui::SetTooltip("%s%s", path.c_str(), exists ? "" : " (missing)");
							}
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					sceneOps.requestNewScene();
				}
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
				{
					sceneOps.requestOpenLoadSceneDialog();
				}
				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
				{
					sceneOps.saveScenePrompt();
				}
				if (ImGui::MenuItem("Save Scene As..."))
				{
					sceneOps.openSaveSceneDialog();
				}

				if (ctx.services != nullptr)
				{
					ImGui::Separator();
					ToolsSession::renderFileNavigationMenuItems(*ctx.services, SCENE_EDITOR, confirmLeave);
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, sceneOps.canUndo()))
				{
					sceneOps.undo();
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z / Ctrl+Y", false, sceneOps.canRedo()))
				{
					sceneOps.redo();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete Selected", "Del / X", false, ctx.hasSelection()))
				{
					sceneOps.deleteSelected();
				}
				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, ctx.selectedEntity != INVALID_ENTITY))
				{
					sceneOps.duplicateSelected();
				}
				if (ImGui::MenuItem("Deselect", "Esc", false, ctx.hasSelection()))
				{
					sceneOps.deselectAll();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Group Selected", "Ctrl+G", false, sceneOps.canGroupSelected()))
				{
					sceneOps.groupSelected();
				}
				if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G", false, ctx.selectedGroupId != 0))
				{
					sceneOps.ungroupSelected();
				}
				if (ImGui::MenuItem("New Empty Group", nullptr))
				{
					sceneOps.createGroup();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Move Individual Rigidbodies", "C", &ctx.selectIndividualClusterEntities))
				{
				}
				if (ImGui::MenuItem("Reload Selected Cluster (from file)", nullptr, false, ctx.selectedClusterId != 0))
				{
					sceneOps.reloadCluster(ctx.selectedClusterId);
				}
				if (ImGui::MenuItem("Reload All Clusters from Files", nullptr))
				{
					sceneOps.reloadAllClusters();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Reset Camera", "F"))
				{
					viewport.resetCamera(ctx);
				}
				ImGui::MenuItem("Show Grid", nullptr, &ctx.gridEnabled);
				ImGui::MenuItem("Snap to Grid", nullptr, &ctx.gridSnap);
				ImGui::SliderFloat("Grid Size", &ctx.gridSize, 0.25f, 5.0f, "%.2f");
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Simulation"))
			{
				if (ImGui::MenuItem(ctx.isSimulating ? "Stop Simulation" : "Play Simulation", "Space"))
				{
					sceneOps.toggleSimulation();
				}
				ImGui::Separator();
				ImGui::DragFloat("Play Gravity", &ctx.playGravity, 0.1f, -50.0f, 50.0f, "%.1f");
				ImGui::DragFloat("Play Damping", &ctx.playDamping, 0.001f, 0.0f, 1.0f, "%.3f");
				if (ImGui::MenuItem("Take Snapshot"))
				{
					ctx.services->serialization().saveSnapshot("dev.weirdsnap");
					ctx.hasSnapshot = true;
				}
				if (ImGui::MenuItem("Restore Snapshot", nullptr, false, ctx.hasSnapshot))
				{
					ctx.services->serialization().loadSnapshot("dev.weirdsnap");
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tools"))
			{
				if (ctx.services != nullptr)
				{
					ToolsSession::renderToolsMenuItems(*ctx.services, SCENE_EDITOR, confirmLeave);
					ImGui::Separator();
				}
				if (ImGui::MenuItem("Reload All Clusters from Files"))
				{
					sceneOps.reloadAllClusters();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("+ New Cluster in Assets..."))
				{
					ctx.openNewClusterModal = true;
				}
				if (ImGui::MenuItem("+ New Shape in Assets..."))
				{
					ctx.openNewShapeModal = true;
				}
				ImGui::EndMenu();
			}

			// Project status banner on the right of the menu bar
			std::string statusText;
			if (ctx.project.isOpen())
			{
				statusText = "Project: " + ctx.project.getRoot().filename().string();
			}
			else
			{
				statusText = "No Project Loaded";
			}

			if (!ctx.currentScenePath.empty())
			{
				statusText += " | Scene: " + std::filesystem::path(ctx.currentScenePath).filename().string();
			}
			else
			{
				statusText += " | Untitled Scene";
			}
			if (ctx.isSceneDirty)
			{
				statusText += "*";
			}

			if (ctx.services != nullptr)
			{
				ToolsSession::renderQuickBackMenuButton(*ctx.services, SCENE_EDITOR, confirmLeave);
			}

			float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
			float rightX = (std::max)(ImGui::GetCursorPosX() + 12.0f, ImGui::GetWindowWidth() - textWidth - 20.0f);
			ImGui::SameLine(rightX);
			ImGui::TextDisabled("%s", statusText.c_str());

			ImGui::EndMainMenuBar();
		}
	}
} // namespace WeirdEngine::Tools::SceneEd
