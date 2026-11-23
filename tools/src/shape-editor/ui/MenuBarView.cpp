#include "shape-editor/ui/MenuBarView.h"

#include "imnodes.h"
#include "shape-editor/model/NodeGraphPresets.h"
#include <algorithm>
#include <filesystem>
#include <imgui.h>
#include <SDL3/SDL_events.h>

#ifndef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
#include "ToolsSession.h"
#endif

namespace WeirdEngine::Tools::ShapeEd
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

	void MenuBarView::render(ServiceProvider& services, NodeGraph& graph, EditorFileManager& fileManager,
							 bool& outDirty, bool& outSyncPositionsToImNodes, bool& hasUnsavedChanges,
							 const std::function<void(std::function<void()>)>& confirmAction,
							 const UndoHooks* undoHooks)
	{
#ifdef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
		(void)services;
#endif
		const auto runConfirmed = [&confirmAction](std::function<void()> action)
		{
			if (confirmAction)
				confirmAction(std::move(action));
			else if (action)
				action();
		};
		const std::function<void()> onResetGraph = undoHooks ? undoHooks->onResetGraph : std::function<void()>{};

		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Graph", "Ctrl+N"))
				{
					runConfirmed(
						[&graph, &fileManager, &outDirty, &outSyncPositionsToImNodes, &hasUnsavedChanges,
						 onResetGraph]()
						{
							graph.clear();
							graph.addNode("sdf_output", {400.0f, 200.0f});
							fileManager.setCurrentFilePath("");
							fileManager.setSuppressRecentForCurrentFile(false);
							outDirty = true;
							outSyncPositionsToImNodes = true;
							hasUnsavedChanges = false;
							if (onResetGraph)
								onResetGraph();
						});
				}
				if (ImGui::MenuItem("Open Shape...", "Ctrl+O"))
				{
					runConfirmed([&fileManager]() { fileManager.openLoadFileDialog(); });
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
						fileManager.clearRecentFiles();
					}
					ImGui::EndMenu();

					if (!toLoad.empty())
					{
						runConfirmed(
							[&graph, &fileManager, toLoad, &outDirty, &outSyncPositionsToImNodes, &hasUnsavedChanges,
							 onResetGraph]()
							{
								if (fileManager.loadGraphFromFile(graph, toLoad))
								{
									outDirty = true;
									outSyncPositionsToImNodes = true;
									hasUnsavedChanges = false;
									if (onResetGraph)
										onResetGraph();
								}
							});
					}
				}
				const bool hasFile = fileManager.hasCurrentFilePath();
				if (ImGui::MenuItem(hasFile ? "Save Shape" : "Save Shape...", "Ctrl+S"))
				{
					if (fileManager.saveGraph(graph))
					{
						hasUnsavedChanges = false;
						if (undoHooks && undoHooks->onSaved)
							undoHooks->onSaved();
					}
				}
				if (ImGui::MenuItem("Save Shape As...", "Ctrl+Shift+S"))
				{
					fileManager.openSaveFileDialog();
				}
				ImGui::Separator();
				if (ImGui::BeginMenu("Presets"))
				{
					auto applyPreset = [&](auto presetFn)
					{
						runConfirmed(
							[&graph, &outDirty, &outSyncPositionsToImNodes, &hasUnsavedChanges, presetFn]()
							{
								presetFn(graph);
								outSyncPositionsToImNodes = true;
								ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
								outDirty = true;
								hasUnsavedChanges = true;
							});
					};

					if (ImGui::MenuItem("Circle"))
					{
						applyPreset(NodeGraphPresets::loadCircle);
					}
					if (ImGui::MenuItem("Ring"))
					{
						applyPreset(NodeGraphPresets::loadCsgRing);
					}
					if (ImGui::MenuItem("Infinite Circle Grid"))
					{
						applyPreset(NodeGraphPresets::loadInfiniteRepeat);
					}
					ImGui::EndMenu();
				}
				ImGui::Separator();
#ifndef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
				ToolsSession::renderFileNavigationMenuItems(services, SCENE_SHAPE_EDITOR, confirmAction);
#else
				if (ImGui::MenuItem("Exit Tool"))
				{
					runConfirmed(
						[]()
						{
							SDL_Event quitEvent;
							quitEvent.type = SDL_EVENT_QUIT;
							SDL_PushEvent(&quitEvent);
						});
				}
#endif
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				const bool canUndo = undoHooks != nullptr && undoHooks->canUndo;
				const bool canRedo = undoHooks != nullptr && undoHooks->canRedo;
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo) && undoHooks && undoHooks->onUndo)
				{
					undoHooks->onUndo();
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z / Ctrl+Y", false, canRedo) && undoHooks && undoHooks->onRedo)
				{
					undoHooks->onRedo();
				}
				ImGui::Separator();
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
				bool nodeAdded = false;
				renderAddNodeMenu(graph, ImVec2(350.0f, 200.0f), nodeAdded);
				if (nodeAdded)
				{
					outDirty = true;
					hasUnsavedChanges = true;
				}
				ImGui::EndMenu();
			}

#ifndef WEIRD_TOOLS_SHAPE_EDITOR_ONLY
			if (ImGui::BeginMenu("Tools"))
			{
				ToolsSession::renderToolsMenuItems(services, SCENE_SHAPE_EDITOR, confirmAction);
				ImGui::EndMenu();
			}

			ToolsSession::renderQuickBackMenuButton(services, SCENE_SHAPE_EDITOR, confirmAction);
#endif

			std::string statusText = fileManager.hasCurrentFilePath()
										 ? std::filesystem::path(fileManager.getCurrentFilePath()).filename().string()
										 : "Untitled Shape";
			if (hasUnsavedChanges)
			{
				statusText += "*";
			}
			float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
			ImGui::SameLine(std::max(ImGui::GetCursorPosX() + 12.0f, ImGui::GetWindowWidth() - textWidth - 20.0f));
			ImGui::TextDisabled("%s", statusText.c_str());

			ImGui::EndMenuBar();
		}
		ImGui::PopStyleColor();
	}
} // namespace WeirdEngine::Tools::ShapeEd
