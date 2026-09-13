#include "ui/MenuBarView.h"

#include "imnodes.h"
#include "model/NodeGraphPresets.h"
#include <filesystem>
#include <imgui.h>
#include <SDL3/SDL_events.h>

namespace WeirdEngine::Editor
{
	void MenuBarView::renderAddNodeMenu(NodeGraph& graph, ImVec2 spawnPos, bool& outDirty)
	{
		static const NodeCategory categories[] = {
			NodeCategory::Input,		NodeCategory::Vector, NodeCategory::Transforms, NodeCategory::Primitives2D,
			NodeCategory::Primitives3D, NodeCategory::CSG,	  NodeCategory::MathUnary,	NodeCategory::MathBinary,
			NodeCategory::MathTernary,	NodeCategory::Output};

		for (auto cat : categories)
		{
			auto defs = NodeRegistry::get().getDefsByCategory(cat);
			if (defs.empty())
				continue;

			if (ImGui::BeginMenu(getCategoryName(cat)))
			{
				for (const auto* def : defs)
				{
					if (ImGui::MenuItem(def->displayName.c_str()))
					{
						int id = graph.addNode(def->typeId, {spawnPos.x, spawnPos.y});
						ImNodes::SetNodeScreenSpacePos(id, spawnPos);
						ImVec2 gridPos = ImNodes::GetNodeGridSpacePos(id);
						if (auto* n = graph.getNode(id))
						{
							n->position = {gridPos.x, gridPos.y};
						}
						outDirty = true;
					}
				}
				ImGui::EndMenu();
			}
		}
	}

	void MenuBarView::render(NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
							 bool& outSyncPositionsToImNodes)
	{
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Graph", "Ctrl+N"))
				{
					graph.clear();
					graph.addNode("sdf_output", {400.0f, 200.0f});
					outDirty = true;
					outSyncPositionsToImNodes = true;
				}
				if (ImGui::MenuItem("Open Graph JSON...", "Ctrl+O"))
				{
					fileManager.openLoadFileDialog();
				}
				const auto& recentFiles = fileManager.getRecentFiles();
				if (ImGui::BeginMenu("Recent", !recentFiles.empty()))
				{
					std::string toLoad;
					for (size_t i = 0; i < recentFiles.size(); ++i)
					{
						const auto& filePath = recentFiles[i];
						std::string filename = std::filesystem::path(filePath).filename().string();
						std::string itemLabel = filename + " (" + filePath + ")";
						if (ImGui::MenuItem(itemLabel.c_str()))
						{
							toLoad = filePath;
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Clear Recent"))
					{
						fileManager.loadRecentFiles(); // Clear in memory by saving empty
						const_cast<std::deque<std::string>&>(recentFiles).clear();
						fileManager.saveRecentFiles();
					}
					ImGui::EndMenu();

					if (!toLoad.empty())
					{
						if (fileManager.loadGraphFromFile(graph, toLoad))
						{
							outDirty = true;
							outSyncPositionsToImNodes = true;
						}
					}
				}
				if (ImGui::MenuItem("Save Graph JSON...", "Ctrl+S"))
				{
					fileManager.openSaveFileDialog();
				}
				ImGui::Separator();
				if (ImGui::BeginMenu("Presets"))
				{
					if (ImGui::MenuItem("Star (Procedural Song)"))
					{
						NodeGraphPresets::loadStar(graph);
						outSyncPositionsToImNodes = true;
						ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
						outDirty = true;
					}
					if (ImGui::MenuItem("Aquatic Wave (Procedural Song)"))
					{
						NodeGraphPresets::loadAquaticWave(graph);
						outSyncPositionsToImNodes = true;
						ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
						outDirty = true;
					}
					if (ImGui::MenuItem("CSG Ring (Subtract)"))
					{
						NodeGraphPresets::loadCsgRing(graph);
						outSyncPositionsToImNodes = true;
						ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
						outDirty = true;
					}
					if (ImGui::MenuItem("Basic Circle"))
					{
						NodeGraphPresets::loadCircle(graph);
						outSyncPositionsToImNodes = true;
						ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
						outDirty = true;
					}
					if (ImGui::MenuItem("Infinite Repeat (Modulo Grid)"))
					{
						NodeGraphPresets::loadInfiniteRepeat(graph);
						outSyncPositionsToImNodes = true;
						ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
						outDirty = true;
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Exit Tool"))
				{
					SDL_Event quitEvent;
					quitEvent.type = SDL_EVENT_QUIT;
					SDL_PushEvent(&quitEvent);
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Auto Layout Nodes", "Ctrl+L"))
				{
					graph.autoLayout();
					outSyncPositionsToImNodes = true;
					ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Zoom In", "Ctrl++"))
				{
					ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
					ImNodes::EditorContextSetZoom(ImNodes::EditorContextGetZoom() * 1.15f, center);
				}
				if (ImGui::MenuItem("Zoom Out", "Ctrl+-"))
				{
					ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
					ImNodes::EditorContextSetZoom(ImNodes::EditorContextGetZoom() / 1.15f, center);
				}
				if (ImGui::MenuItem("Reset Zoom (100%)", "Ctrl+0"))
				{
					ImNodes::EditorContextResetZoom();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Reset View (Pan & Zoom)"))
				{
					ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
					ImNodes::EditorContextResetZoom();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Add Node"))
			{
				renderAddNodeMenu(graph, ImVec2(350.0f, 200.0f), outDirty);
				ImGui::EndMenu();
			}

			ImGui::EndMenuBar();
		}
		ImGui::PopStyleColor();
	}
} // namespace WeirdEngine::Editor
