#include "ui/NodeCanvasView.h"

#include <cmath>
#include <vector>

#include "imnodes.h"
#include "ui/EditorTheme.h"
#include "ui/MenuBarView.h"
#include <imgui.h>

namespace WeirdEngine::Editor
{
	void NodeCanvasView::render(NodeGraph& graph, EditorFileManager& fileManager, bool& outDirty,
								bool& syncPositionsToImNodes)
	{
		ImNodes::BeginNodeEditor();

		// 0. Auto-sync positions to ImNodes if graph was laid out or preset loaded
		bool didSyncPositions = false;
		if (syncPositionsToImNodes)
		{
			for (const auto& [nodeId, node] : graph.getNodes())
			{
				ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.position.x, node.position.y));
			}
			syncPositionsToImNodes = false;
			didSyncPositions = true;
		}

		float zoom = ImNodes::EditorContextGetZoom();

		// 1. Render all nodes
		for (auto& [nodeId, node] : graph.getNodes())
		{
			const NodeDef* def = NodeRegistry::get().findDef(node.typeId);
			if (!def)
				continue;

			ImNodes::BeginNode(node.id);

			// Title Bar
			ImNodes::BeginNodeTitleBar();
			ImVec4 titleColor = EditorTheme::getCategoryColor(def->category);
			if (node.typeId == "param_var")
			{
				int idx = std::clamp(node.data.customInt, 0, 7);
				const std::string& pName = graph.getParameterName(idx);
				if (!pName.empty())
				{
					ImGui::TextColored(titleColor, "%s", pName.c_str());
				}
				else
				{
					ImGui::TextColored(titleColor, "Variable (var%d)", idx);
				}
			}
			else
			{
				ImGui::TextColored(titleColor, "%s", def->displayName.c_str());
			}
			ImNodes::EndNodeTitleBar();

			// Custom Editable Node Parameters (e.g. constant value, var index)
			if (node.typeId == "const_float")
			{
				ImGui::PushItemWidth(85.0f * zoom);
				if (ImGui::DragFloat("##val", &node.data.customFloat, 0.1f, -1000.0f, 1000.0f, "%.2f"))
					outDirty = true;
				ImGui::PopItemWidth();
			}
			else if (node.typeId == "param_var")
			{
				int idx = std::clamp(node.data.customInt, 0, 7);
				std::string pName = graph.getParameterName(idx);
				if (pName.empty())
					pName = "var" + std::to_string(idx);

				std::string sliderFmt;
				for (char c : pName)
				{
					if (c == '%')
						sliderFmt += "%%";
					else
						sliderFmt += c;
				}

				ImGui::PushItemWidth(85.0f * zoom);
				if (ImGui::SliderInt("##idx", &node.data.customInt, 0, 7, sliderFmt.c_str()))
					outDirty = true;
				ImGui::PopItemWidth();
			}

			// Render Input Pins
			for (size_t inIdx = 0; inIdx < def->inputs.size(); ++inIdx)
			{
				const auto& pDef = def->inputs[inIdx];
				int pinId = makePinId(node.id, true, static_cast<int>(inIdx));

				ImNodesPinShape shape =
					(pDef.type == PinType::Float) ? ImNodesPinShape_CircleFilled : ImNodesPinShape_TriangleFilled;

				// Pin Color style
				ImNodes::PushColorStyle(ImNodesCol_Pin, (pDef.type == PinType::Float) ? IM_COL32(60, 200, 220, 255)
																					  : IM_COL32(240, 160, 60, 255));

				ImNodes::BeginInputAttribute(pinId, shape);

				// Check if this input pin has an incoming link
				bool isLinked = false;
				for (const auto& l : graph.getLinks())
				{
					if (l.endPinId == pinId)
					{
						isLinked = true;
						break;
					}
				}

				if (isLinked)
				{
					ImGui::TextUnformatted(pDef.name.c_str());
				}
				else
				{
					// Unlinked: render inline editable widget
					ImGui::PushItemWidth(75.0f * zoom);
					if (pDef.type == PinType::Float)
					{
						if (inIdx < node.inputFloats.size())
						{
							if (ImGui::DragFloat(pDef.name.c_str(), &node.inputFloats[inIdx], pDef.speed, pDef.minFloat,
												 pDef.maxFloat, "%.1f"))
							{
								outDirty = true;
							}
						}
					}
					else
					{
						if (inIdx < node.inputVec2s.size())
						{
							if (ImGui::DragFloat2(pDef.name.c_str(), &node.inputVec2s[inIdx].x, 0.5f, -500.0f, 500.0f,
												  "%.1f"))
							{
								outDirty = true;
							}
						}
					}
					ImGui::PopItemWidth();
				}

				ImNodes::EndInputAttribute();
				ImNodes::PopColorStyle();
			}

			// Spacer separator between inputs / controls and outputs
			if ((!def->inputs.empty() || node.typeId == "const_float" || node.typeId == "param_var") &&
				!def->outputs.empty())
			{
				ImGui::Spacing();
				float nodeWidth = ImNodes::GetNodeDimensions(node.id).x;
				float padding = ImNodes::GetStyle().NodePadding.x * zoom;
				float lineWidth = (nodeWidth > 2.0f * padding) ? (nodeWidth - 2.0f * padding) : (85.0f * zoom);
				if (lineWidth > 0.0f)
				{
					ImVec2 p = ImGui::GetCursorScreenPos();
					ImGui::GetWindowDrawList()->AddLine(p, ImVec2(p.x + lineWidth, p.y),
														ImGui::GetColorU32(ImGuiCol_Separator));
					// Zero width so it never feeds back into ImGui group size
					ImGui::Dummy(ImVec2(0.0f, 2.0f * zoom));
				}
				ImGui::Spacing();
			}

			// Render Output Pins
			for (size_t outIdx = 0; outIdx < def->outputs.size(); ++outIdx)
			{
				const auto& pDef = def->outputs[outIdx];
				int pinId = makePinId(node.id, false, static_cast<int>(outIdx));

				ImNodesPinShape shape =
					(pDef.type == PinType::Float) ? ImNodesPinShape_CircleFilled : ImNodesPinShape_TriangleFilled;

				ImNodes::PushColorStyle(ImNodesCol_Pin, (pDef.type == PinType::Float) ? IM_COL32(60, 200, 220, 255)
																					  : IM_COL32(240, 160, 60, 255));

				ImNodes::BeginOutputAttribute(pinId, shape);
				ImGui::TextUnformatted(pDef.name.c_str());
				ImNodes::EndOutputAttribute();

				ImNodes::PopColorStyle();
			}

			ImNodes::EndNode();
		}

		// 2. Render all links
		for (const auto& link : graph.getLinks())
		{
			ImNodes::Link(link.id, link.startPinId, link.endPinId);
		}

		// MiniMap
		ImNodes::MiniMap(0.18f, ImNodesMiniMapLocation_BottomLeft);

		ImNodes::EndNodeEditor();

		// Save dragged node positions back to node.position
		if (!didSyncPositions)
		{
			for (auto& [nodeId, node] : graph.getNodes())
			{
				ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
				node.position = {pos.x, pos.y};
			}
		}

		// 3. Link creation interaction
		int startAttr, endAttr;
		if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
		{
			if (graph.addLink(startAttr, endAttr))
			{
				outDirty = true;
			}
		}

		// 4. Link destruction interaction
		int linkId;
		if (ImNodes::IsLinkDestroyed(&linkId))
		{
			graph.removeLink(linkId);
			outDirty = true;
		}

		// 5. Delete key handling for selected nodes/links
		if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_X))
		{
			int numNodes = ImNodes::NumSelectedNodes();
			if (numNodes > 0)
			{
				std::vector<int> selectedNodes(numNodes);
				ImNodes::GetSelectedNodes(selectedNodes.data());
				for (int id : selectedNodes)
				{
					// Do not delete the main output node
					if (id != graph.getOutputNodeId())
					{
						graph.removeNode(id);
						outDirty = true;
					}
				}
				ImNodes::ClearNodeSelection();
			}

			int numLinks = ImNodes::NumSelectedLinks();
			if (numLinks > 0)
			{
				std::vector<int> selectedLinks(numLinks);
				ImNodes::GetSelectedLinks(selectedLinks.data());
				for (int id : selectedLinks)
				{
					graph.removeLink(id);
					outDirty = true;
				}
				ImNodes::ClearLinkSelection();
			}
		}

		// 6. Right-click context popup on canvas for quick node spawning
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
			!ImNodes::IsNodeHovered(nullptr) && !ImNodes::IsLinkHovered(nullptr))
		{
			ImGui::OpenPopup("NodeCanvasContextMenu");
		}

		if (ImGui::BeginPopup("NodeCanvasContextMenu"))
		{
			ImVec2 mousePos = ImGui::GetMousePosOnOpeningCurrentPopup();
			MenuBarView::renderAddNodeMenu(graph, mousePos, outDirty);
			ImGui::EndPopup();
		}

		// 7. Shortcut: Ctrl+L to trigger Auto Layout
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
		{
			graph.autoLayout();
			syncPositionsToImNodes = true;
			ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
		}

		// 8. Shortcuts: Zoom in/out/reset
		if (ImGui::GetIO().KeyCtrl)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Equal) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd))
			{
				ImVec2 center = ImGui::GetMousePos();
				ImNodes::EditorContextSetZoom(ImNodes::EditorContextGetZoom() * 1.15f, center);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_Minus) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract))
			{
				ImVec2 center = ImGui::GetMousePos();
				ImNodes::EditorContextSetZoom(ImNodes::EditorContextGetZoom() / 1.15f, center);
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_0) || ImGui::IsKeyPressed(ImGuiKey_Keypad0))
			{
				ImNodes::EditorContextResetZoom();
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_S))
			{
				fileManager.openSaveFileDialog();
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_O))
			{
				fileManager.openLoadFileDialog();
			}
		}

		// 9. Floating Canvas HUD overlay (Top-Right of Canvas)
		{
			float currentZoom = ImNodes::EditorContextGetZoom();
			int zoomPct = static_cast<int>(std::round(currentZoom * 100.0f));

			ImVec2 winPos = ImGui::GetWindowPos();
			ImVec2 winSize = ImGui::GetWindowSize();
			float hudW = 190.0f;
			ImVec2 hudPos(winPos.x + winSize.x - hudW - 16.0f, winPos.y + 12.0f);

			ImGui::SetNextWindowPos(hudPos, ImGuiCond_Always);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.14f, 0.92f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 0.85f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

			ImGuiWindowFlags hudFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
										ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
										ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

			if (ImGui::Begin("##CanvasZoomHUD", nullptr, hudFlags))
			{
				if (ImGui::Button(" - "))
				{
					ImVec2 center = ImVec2(winPos.x + winSize.x * 0.5f, winPos.y + winSize.y * 0.5f);
					ImNodes::EditorContextSetZoom(currentZoom / 1.15f, center);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Zoom Out (Ctrl+Minus or Wheel Down)");

				ImGui::SameLine();
				char zoomBuf[16];
				snprintf(zoomBuf, sizeof(zoomBuf), "%3d%%", zoomPct);
				if (ImGui::Button(zoomBuf))
				{
					ImNodes::EditorContextResetZoom();
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Reset Zoom to 100%% (Ctrl+0)");

				ImGui::SameLine();
				if (ImGui::Button(" + "))
				{
					ImVec2 center = ImVec2(winPos.x + winSize.x * 0.5f, winPos.y + winSize.y * 0.5f);
					ImNodes::EditorContextSetZoom(currentZoom * 1.15f, center);
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Zoom In (Ctrl+Plus or Wheel Up)");

				ImGui::SameLine();
				if (ImGui::Button("Layout"))
				{
					graph.autoLayout();
					syncPositionsToImNodes = true;
					ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Auto Layout Graph (Ctrl+L)");

				ImGui::End();
			}

			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor(2);
		}
	}
} // namespace WeirdEngine::Editor
