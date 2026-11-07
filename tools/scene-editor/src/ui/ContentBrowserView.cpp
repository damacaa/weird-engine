#include "ui/ContentBrowserView.h"
#include "services/ExternalToolsManager.h"
#include "services/SceneOperations.h"

#include "weird-engine.h"

#include <algorithm>
#include <imgui.h>
#include <string>

namespace WeirdEngine::Editor
{
	void ContentBrowserView::renderAssetBrowserTab(EditorContext& ctx, SceneOperations& sceneOps,
												   ExternalToolsManager& toolsMgr)
	{
		if (!ctx.project.isOpen())
		{
			ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "No project open.");
			if (ImGui::Button("Open Project Folder...", ImVec2(-1.0f, 30.0f)))
			{
				ctx.project.openFolderDialog();
			}
			return;
		}

		// Filter buttons
		const char* filters[] = {"All", "Scenes", "Clusters", "Shapes"};
		ImGui::Text("Filter:");
		ImGui::SameLine();
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			bool active = (ctx.assetFilterType == i);
			if (active)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
			if (ImGui::Button(filters[i]))
			{
				ctx.assetFilterType = i;
			}
			if (active)
				ImGui::PopStyleColor();
		}

		// Search
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##AssetSearch", "Search assets...", ctx.assetSearchBuf, sizeof(ctx.assetSearchBuf));

		ImGui::Separator();

		// Rescan button
		if (ImGui::Button("Rescan Folder", ImVec2(-1.0f, 22.0f)))
		{
			ctx.project.scanAssets();
		}

		ImGui::BeginChild("##AssetList", ImVec2(0.0f, 0.0f), true);

		std::string searchStr(ctx.assetSearchBuf);
		std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

		auto matchesSearch = [&](const Editor::AssetInfo& item)
		{
			if (searchStr.empty())
				return true;
			std::string nameLower = item.fileName;
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
			if (nameLower.find(searchStr) != std::string::npos)
				return true;
			std::string relLower = item.relativePath;
			std::transform(relLower.begin(), relLower.end(), relLower.begin(), ::tolower);
			return relLower.find(searchStr) != std::string::npos;
		};

		// 1. Scenes
		if (ctx.assetFilterType == 0 || ctx.assetFilterType == 1)
		{
			if (ImGui::CollapsingHeader("Scenes (.weirdscene)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& sceneAsset : ctx.project.getScenes())
				{
					if (!matchesSearch(sceneAsset))
						continue;
					ImGui::Bullet();
					if (ImGui::Selectable(sceneAsset.fileName.c_str()))
					{
						sceneOps.loadScene(sceneAsset.fullPath.string());
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Click to load: %s", sceneAsset.relativePath.c_str());
					}
				}
			}
		}

		// 2. Clusters
		if (ctx.assetFilterType == 0 || ctx.assetFilterType == 2)
		{
			if (ImGui::CollapsingHeader("Clusters (.weirdcluster)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("+ New Cluster in Assets", ImVec2(-1.0f, 22.0f)))
				{
					ctx.openNewClusterModal = true;
				}

				for (const auto& clusterAsset : ctx.project.getClusters())
				{
					if (!matchesSearch(clusterAsset))
						continue;
					ImGui::PushID(clusterAsset.fullPath.string().c_str());
					ImGui::Bullet();
					ImGui::Text("%s", clusterAsset.fileName.c_str());
					if (ImGui::IsItemHovered() && !clusterAsset.relativePath.empty())
					{
						ImGui::SetTooltip("%s", clusterAsset.relativePath.c_str());
					}
					ImGui::SameLine(ImGui::GetContentRegionAvail().x - 145.0f);
					if (ImGui::SmallButton("Spawn"))
					{
						sceneOps.spawnCluster(clusterAsset.fullPath.string(), sceneOps.getCameraCenterWorld());
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Edit"))
					{
						toolsMgr.launchClusterEditor(clusterAsset.fullPath.string());
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Reload"))
					{
						sceneOps.reloadClustersFromAsset(clusterAsset.fullPath.string());
					}
					if (ImGui::BeginPopupContextItem())
					{
						if (ImGui::MenuItem("Spawn in Scene"))
						{
							sceneOps.spawnCluster(clusterAsset.fullPath.string(), sceneOps.getCameraCenterWorld());
						}
						if (ImGui::MenuItem("Edit in Cluster Editor"))
						{
							toolsMgr.launchClusterEditor(clusterAsset.fullPath.string());
						}
						if (ImGui::MenuItem("Reload Instances in Scene"))
						{
							sceneOps.reloadClustersFromAsset(clusterAsset.fullPath.string());
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}
		}

		// 3. Custom Shapes
		if (ctx.assetFilterType == 0 || ctx.assetFilterType == 3)
		{
			if (ImGui::CollapsingHeader("Custom Shapes (.weirdshape)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("+ New Shape in Assets", ImVec2(-1.0f, 22.0f)))
				{
					ctx.openNewShapeModal = true;
				}

				for (const auto& shapeAsset : ctx.project.getCustomShapes())
				{
					if (!matchesSearch(shapeAsset))
						continue;
					ImGui::PushID(shapeAsset.fullPath.string().c_str());
					ImGui::Bullet();
					ImGui::Text("%s", shapeAsset.fileName.c_str());
					if (ImGui::IsItemHovered() && !shapeAsset.relativePath.empty())
					{
						ImGui::SetTooltip("%s", shapeAsset.relativePath.c_str());
					}
					ImGui::SameLine(ImGui::GetContentRegionAvail().x - 140.0f);
					if (ImGui::SmallButton("Add"))
					{
						sceneOps.spawnCustomShape(shapeAsset.fullPath.string(), sceneOps.getCameraCenterWorld());
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Edit"))
					{
						toolsMgr.launchShapeEditor(shapeAsset.fullPath.string());
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("Reload"))
					{
						sceneOps.reloadCustomShape(shapeAsset.fullPath.string());
					}
					if (ImGui::BeginPopupContextItem())
					{
						if (ImGui::MenuItem("Add to Scene"))
						{
							sceneOps.spawnCustomShape(shapeAsset.fullPath.string(), sceneOps.getCameraCenterWorld());
						}
						if (ImGui::MenuItem("Edit in Shape Editor"))
						{
							toolsMgr.launchShapeEditor(shapeAsset.fullPath.string());
						}
						if (ImGui::MenuItem("Reload in Scene"))
						{
							sceneOps.reloadCustomShape(shapeAsset.fullPath.string());
						}
						ImGui::EndPopup();
					}
					ImGui::PopID();
				}
			}
		}

		// Modals for creating new assets
		if (ctx.openNewClusterModal)
		{
			ImGui::OpenPopup("Create New Cluster");
			ctx.openNewClusterModal = false;
		}
		if (ImGui::BeginPopupModal("Create New Cluster", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter name for the new cluster asset:");
			bool enterPressed = ImGui::InputText("##NewClusterName", ctx.newClusterNameBuf,
												 sizeof(ctx.newClusterNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::Spacing();
			if (ImGui::Button("Create & Open", ImVec2(120.0f, 0.0f)) || enterPressed)
			{
				toolsMgr.createNewClusterAsset(ctx.newClusterNameBuf);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		if (ctx.openNewShapeModal)
		{
			ImGui::OpenPopup("Create New Shape");
			ctx.openNewShapeModal = false;
		}
		if (ImGui::BeginPopupModal("Create New Shape", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter name for the new custom shape asset:");
			bool enterPressed = ImGui::InputText("##NewShapeName", ctx.newShapeNameBuf, sizeof(ctx.newShapeNameBuf),
												 ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::Spacing();
			if (ImGui::Button("Create & Open", ImVec2(120.0f, 0.0f)) || enterPressed)
			{
				toolsMgr.createNewShapeAsset(ctx.newShapeNameBuf);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}

		ImGui::EndChild();
	}
} // namespace WeirdEngine::Editor
