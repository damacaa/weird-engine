#include "ui/MenuBarView.h"
#include "services/ExternalToolsManager.h"
#include "services/SceneOperations.h"
#include "ui/ViewportInteraction.h"

#include "weird-engine.h"

#include <cstdlib>
#include <filesystem>
#include <imgui.h>

namespace WeirdEngine::Editor
{
	void MenuBarView::renderMenuBar(EditorContext& ctx, SceneOperations& sceneOps, ExternalToolsManager& toolsMgr,
									ViewportInteraction& viewport)
	{
		if (ImGui::BeginMainMenuBar())
		{
			ctx.menuBarHeight = ImGui::GetWindowSize().y;

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("Open Project Folder..."))
				{
					ctx.project.openFolderDialog();
				}

				if (ImGui::BeginMenu("Recent Projects"))
				{
					if (ctx.project.getRecentProjects().empty())
					{
						ImGui::MenuItem("No Recent Projects", nullptr, false, false);
					}
					else
					{
						for (const auto& path : ctx.project.getRecentProjects())
						{
							if (ImGui::MenuItem(path.c_str()))
							{
								ctx.project.open(path);
							}
						}
					}
					ImGui::EndMenu();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("New Scene", "Ctrl+N"))
				{
					sceneOps.newScene();
				}
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
				{
					sceneOps.openLoadSceneDialog();
				}
				if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
				{
					sceneOps.saveScenePrompt();
				}
				if (ImGui::MenuItem("Save Scene As..."))
				{
					sceneOps.openSaveSceneDialog();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Exit"))
				{
					std::exit(0);
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
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
				if (ImGui::MenuItem("Reset Cluster to Initial Position", "R", false, ctx.selectedClusterId != 0))
				{
					auto* c = ctx.services->clusters().getById(ctx.selectedClusterId);
					if (c)
					{
						sceneOps.resetClusterToInitialPosition(*c);
					}
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
				if (ImGui::MenuItem("Reload All Clusters from Files"))
				{
					sceneOps.reloadAllClusters();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Launch Cluster Editor"))
				{
					toolsMgr.launchClusterEditor();
				}
				if (ImGui::MenuItem("Launch Shape Editor"))
				{
					toolsMgr.launchShapeEditor();
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

			float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
			ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - 20.0f);
			ImGui::TextDisabled("%s", statusText.c_str());

			ImGui::EndMainMenuBar();
		}
	}
} // namespace WeirdEngine::Editor
