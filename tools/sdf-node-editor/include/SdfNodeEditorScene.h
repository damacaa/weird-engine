#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <SDL3/SDL_dialog.h>
#include <weird-engine.h>

#include "imnodes.h"
#include "NodeGraph.h"
#include "NodeRegistry.h"

namespace WeirdEngine::Editor
{
	inline const char* getScaleName(WeirdRenderer::MusicalScale scale)
	{
		switch (scale)
		{
			case WeirdRenderer::MusicalScale::PentatonicMajor:
				return "Pentatonic Major";
			case WeirdRenderer::MusicalScale::PentatonicMinor:
				return "Pentatonic Minor";
			case WeirdRenderer::MusicalScale::Major:
				return "Major";
			case WeirdRenderer::MusicalScale::NaturalMinor:
				return "Natural Minor";
			case WeirdRenderer::MusicalScale::Dorian:
				return "Dorian";
			case WeirdRenderer::MusicalScale::Lydian:
				return "Lydian";
			case WeirdRenderer::MusicalScale::Chromatic:
				return "Chromatic";
			default:
				return "Unknown";
		}
	}

	inline std::string midiToNoteString(int midi)
	{
		if (midi < 0 || midi > 127)
			return "N/A";
		const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		int note = midi % 12;
		int octave = (midi / 12) - 1;
		return std::string(noteNames[note]) + std::to_string(octave);
	}

	class SdfNodeEditorScene : public Scene2D
	{
	public:
		SdfNodeEditorScene() = default;
		~SdfNodeEditorScene() override = default;

	protected:
		void onCreate(Registry& registry, ServiceProvider& services) override
		{
			// Set neutral dark gray background for the 2D scene
			auto& bg = services.render().getBackground();
			bg.type = BackgroundType::Solid;
			bg.primaryColor = glm::vec4(0.08f, 0.08f, 0.08f, 1.0f);

			// Define material 0 with default light blue accent color
			auto& mat0 = services.materials2D().get(0);
			mat0.name = "sdf_preview";
			mat0.color = glm::vec4(0.15f, 0.85f, 0.95f, 1.0f);
			mat0.emission = 0.45f;

			// Load default starter graph
			m_graph.loadPresetMandalaStar();
			m_dirty = true;
		}

		void onStart(Registry& registry, ServiceProvider& services) override
		{
			// Ensure main camera has non-zero depth to prevent division-by-zero NaNs
			Entity mainCam = services.tags().getEntityByTag("mainCamera");
			if (mainCam < MAX_ENTITIES && registry.hasComponent<Transform>(mainCam))
			{
				auto& t = registry.getComponent<Transform>(mainCam);
				if (t.position.z == 0.0f)
				{
					t.position.z = 35.0f;
				}
			}

			ImNodes::CreateContext();
			ImNodes::StyleColorsDark();

			// Setup neutral gray node styling
			ImNodesStyle& style = ImNodes::GetStyle();
			style.Flags |= ImNodesStyleFlags_GridLines | ImNodesStyleFlags_NodeOutline;
			style.NodeCornerRounding = 6.0f;
			style.NodeBorderThickness = 1.5f;
			style.Colors[ImNodesCol_GridBackground] = IM_COL32(20, 20, 20, 255);
			style.Colors[ImNodesCol_GridLine] = IM_COL32(36, 36, 36, 255);
			style.Colors[ImNodesCol_GridLinePrimary] = IM_COL32(50, 50, 50, 255);
			style.Colors[ImNodesCol_TitleBar] = IM_COL32(38, 38, 38, 255);
			style.Colors[ImNodesCol_TitleBarHovered] = IM_COL32(52, 52, 52, 255);
			style.Colors[ImNodesCol_TitleBarSelected] = IM_COL32(66, 66, 66, 255);
			style.Colors[ImNodesCol_NodeBackground] = IM_COL32(26, 26, 26, 255);
			style.Colors[ImNodesCol_NodeBackgroundHovered] = IM_COL32(34, 34, 34, 255);
			style.Colors[ImNodesCol_NodeBackgroundSelected] = IM_COL32(42, 42, 42, 255);
			style.Colors[ImNodesCol_Link] = IM_COL32(90, 170, 240, 210);
			style.Colors[ImNodesCol_LinkHovered] = IM_COL32(130, 205, 255, 255);
			style.Colors[ImNodesCol_LinkSelected] = IM_COL32(255, 195, 75, 255);
			style.Colors[ImNodesCol_Pin] = IM_COL32(70, 195, 215, 255);
			style.Colors[ImNodesCol_PinHovered] = IM_COL32(110, 235, 255, 255);

			// Layout starter graph and sync to ImNodes
			m_graph.autoLayout();
			m_syncPositionsToImNodes = true;

			// Synchronize shape visually and audio
			rebuildAndSync(services);

			services.audio().music().setPlaying(m_playSong);
			services.audio().music().setVolume(m_musicVolume);
		}

		void onDestroy(Registry& registry, ServiceProvider& services) override
		{
			ImNodes::DestroyContext();
		}

		void onUpdate(Registry& registry, ServiceProvider& services) override
		{
			{
				std::string loadPath;
				std::string savePath;
				{
					std::lock_guard<std::mutex> lock(m_fileActionMutex);
					if (!m_pendingLoadPath.empty())
					{
						loadPath = m_pendingLoadPath;
						m_pendingLoadPath.clear();
					}
					if (!m_pendingSavePath.empty())
					{
						savePath = m_pendingSavePath;
						m_pendingSavePath.clear();
					}
				}
				if (!loadPath.empty())
				{
					loadGraphFromFile(loadPath);
				}
				if (!savePath.empty())
				{
					saveGraphToFile(savePath);
				}
			}

			if (m_dirty)
			{
				rebuildAndSync(services);
				m_dirty = false;
			}
		}

		void onCustomUI(Registry& registry, ServiceProvider& services) override
		{
			renderEditorUI(services);
		}

	private:
		void rebuildAndSync(ServiceProvider& services)
		{
			int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
			int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;
			float menuH = 24.0f;
			float contentH = static_cast<float>(winH) - menuH;
			float inspectorWidth = 360.0f;
			float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
			glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);

			m_evaluatedExpr = m_graph.evaluate();
			if (!m_evaluatedExpr.node)
			{
				m_evaluatedExpr = Expr(0.0f);
			}

			// 1. Update or create Procedural Song centered on the preview gap
			if (m_song)
			{
				m_song->setCenter(targetCenter);
				m_song->setShapeExpression(m_evaluatedExpr.node);
				m_song->calculateMusicalPropertiesFromShape();
			}
			else
			{
				m_song = WeirdRenderer::SdfSong::create("node_editor_song", m_evaluatedExpr, targetCenter);
			}

			// 2. Register with AudioService for procedural UI shape rendering
			SongVisualizationOptions visualOptions;
			visualOptions.material = 0;
			visualOptions.combination = CombinationType::Addition;
			visualOptions.group = 0;

			m_previewEntity = services.audio().setSong(m_song, visualOptions);
			if (m_previewEntity != INVALID_ENTITY && services.registry().hasComponent<UIShape>(m_previewEntity))
			{
				auto& uiShape = services.registry().getComponent<UIShape>(m_previewEntity);
				uiShape.smoothFactor = 0.0f;
				uiShape.material = 0;
				services.registry().setComponentDirty(uiShape);
			}

			services.render().forceShaderRefreshUI();
			services.audio().resampleShape();

			// 3. Cache GLSL & C++ code
			m_cachedGlslCode = m_evaluatedExpr.node ? m_evaluatedExpr.node->print() : "0.0";
			m_cachedCppCode = m_graph.generateCppCode();
		}

		void renderEditorUI(ServiceProvider& services)
		{
			int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
			int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;

			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)), ImGuiCond_Always);

			// Enable NoBackground so the underlying OpenGL ES world scene is visible through gaps
			ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
									 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			if (ImGui::Begin("##NodeEditorMainSpace", nullptr, flags))
			{
				ImGui::PopStyleVar(3);

				renderMenuBar(services);

				float menuH = ImGui::GetFrameHeight();
				float contentH = static_cast<float>(winH) - menuH;

				float inspectorWidth = 360.0f;
				float canvasWidth = static_cast<float>(winW) - inspectorWidth;
				float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
				float inspectorHeight = contentH - previewHeight;

				// Keep song centered inside preview gap if window dimensions change
				glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);
				if (m_song && glm::distance(m_song->getCenter(), targetCenter) > 1.0f)
				{
					rebuildAndSync(services);
				}

				// =========================================================================
				// 1. Interactive Node Editor Canvas (Draws all the way from the left edge)
				// =========================================================================
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				ImGui::BeginChild("##NodeCanvasChild", ImVec2(canvasWidth, contentH), false,
								  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				renderNodeCanvas();
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();

				// Seamlessly place inspector next to canvas without any gap
				ImGui::SameLine(0.0f, 0.0f);

				// =========================================================================
				// 2. Right Column: Inspector (shorter to create the preview gap below)
				// =========================================================================
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

				ImGui::BeginChild("##InspectorChild", ImVec2(0.0f, inspectorHeight), true);
				renderInspector(services);
				ImGui::EndChild();

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(2);

				// Frame the transparent preview gap with a clean matching border
				ImVec2 gapMin(canvasWidth, menuH + inspectorHeight);
				ImVec2 gapMax(static_cast<float>(winW), static_cast<float>(winH));
				ImGui::GetWindowDrawList()->AddRect(gapMin, gapMax, IM_COL32(55, 55, 55, 255));
			}
			else
			{
				ImGui::PopStyleVar(3);
			}
			ImGui::End();
		}

		void renderMenuBar(ServiceProvider& services)
		{
			ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
			if (ImGui::BeginMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("New Graph", "Ctrl+N"))
					{
						m_graph.clear();
						m_graph.addNode("sdf_output", {400.0f, 200.0f});
						m_dirty = true;
					}
					if (ImGui::MenuItem("Open Graph JSON...", "Ctrl+O"))
					{
						openLoadFileDialog();
					}
					if (ImGui::MenuItem("Save Graph JSON...", "Ctrl+S"))
					{
						openSaveFileDialog();
					}
					ImGui::Separator();
					if (ImGui::BeginMenu("Presets"))
					{
						if (ImGui::MenuItem("Mandala Star (Procedural Song)"))
						{
							m_graph.loadPresetMandalaStar();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("Aquatic Wave (Procedural Song)"))
						{
							m_graph.loadPresetAquaticWave();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("CSG Ring (Subtract)"))
						{
							m_graph.loadPresetCsgRing();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("Basic Circle"))
						{
							m_graph.loadPresetCircle();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
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
						m_graph.autoLayout();
						m_syncPositionsToImNodes = true;
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
					renderAddNodeMenu(ImVec2(350.0f, 200.0f));
					ImGui::EndMenu();
				}

				ImGui::EndMenuBar();
			}
			ImGui::PopStyleColor();
		}

		void renderAddNodeMenu(ImVec2 spawnPos)
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
							int id = m_graph.addNode(def->typeId, {spawnPos.x, spawnPos.y});
							ImNodes::SetNodeScreenSpacePos(id, spawnPos);
							m_dirty = true;
						}
					}
					ImGui::EndMenu();
				}
			}
		}

		void renderNodeCanvas()
		{
			ImNodes::BeginNodeEditor();

			// 0. Auto-sync positions to ImNodes if graph was laid out or preset loaded
			if (m_syncPositionsToImNodes)
			{
				for (const auto& [nodeId, node] : m_graph.getNodes())
				{
					ImNodes::SetNodeGridSpacePos(node.id, ImVec2(node.position.x, node.position.y));
				}
				m_syncPositionsToImNodes = false;
			}

			// 1. Render all nodes
			for (auto& [nodeId, node] : m_graph.getNodes())
			{
				const NodeDef* def = NodeRegistry::get().findDef(node.typeId);
				if (!def)
					continue;

				ImNodes::BeginNode(node.id);

				// Title Bar
				ImNodes::BeginNodeTitleBar();
				ImVec4 titleColor = getCategoryColor(def->category);
				ImGui::TextColored(titleColor, "%s", def->displayName.c_str());
				ImNodes::EndNodeTitleBar();

				// Custom Editable Node Parameters (e.g. constant value, var index)
				if (node.typeId == "const_float")
				{
					ImGui::PushItemWidth(85.0f);
					if (ImGui::DragFloat("##val", &node.data.customFloat, 0.1f, -1000.0f, 1000.0f, "%.2f"))
						m_dirty = true;
					ImGui::PopItemWidth();
				}
				else if (node.typeId == "param_var")
				{
					ImGui::PushItemWidth(70.0f);
					if (ImGui::SliderInt("##idx", &node.data.customInt, 0, 7, "var%d"))
						m_dirty = true;
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
					ImNodes::PushColorStyle(ImNodesCol_Pin, (pDef.type == PinType::Float)
																? IM_COL32(60, 200, 220, 255)
																: IM_COL32(240, 160, 60, 255));

					ImNodes::BeginInputAttribute(pinId, shape);

					// Check if this input pin has an incoming link
					bool isLinked = false;
					for (const auto& l : m_graph.getLinks())
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
						ImGui::PushItemWidth(75.0f);
						if (pDef.type == PinType::Float)
						{
							if (inIdx < node.inputFloats.size())
							{
								if (ImGui::DragFloat(pDef.name.c_str(), &node.inputFloats[inIdx], pDef.speed,
													 pDef.minFloat, pDef.maxFloat, "%.1f"))
								{
									m_dirty = true;
								}
							}
						}
						else
						{
							if (inIdx < node.inputVec2s.size())
							{
								if (ImGui::DragFloat2(pDef.name.c_str(), &node.inputVec2s[inIdx].x, 0.5f, -500.0f,
													  500.0f, "%.1f"))
								{
									m_dirty = true;
								}
							}
						}
						ImGui::PopItemWidth();
					}

					ImNodes::EndInputAttribute();
					ImNodes::PopColorStyle();
				}

				// Render Output Pins
				for (size_t outIdx = 0; outIdx < def->outputs.size(); ++outIdx)
				{
					const auto& pDef = def->outputs[outIdx];
					int pinId = makePinId(node.id, false, static_cast<int>(outIdx));

					ImNodesPinShape shape =
						(pDef.type == PinType::Float) ? ImNodesPinShape_CircleFilled : ImNodesPinShape_TriangleFilled;

					ImNodes::PushColorStyle(ImNodesCol_Pin, (pDef.type == PinType::Float)
																? IM_COL32(60, 200, 220, 255)
																: IM_COL32(240, 160, 60, 255));

					ImNodes::BeginOutputAttribute(pinId, shape);
					float nodeWidth = ImNodes::GetNodeDimensions(node.id).x;
					if (nodeWidth <= 0.0f)
					{
						nodeWidth = 140.0f;
					}
					float labelWidth = ImGui::CalcTextSize(pDef.name.c_str()).x;
					float offset = nodeWidth - labelWidth - 20.0f;
					if (offset > 0.0f)
					{
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
					}
					ImGui::TextUnformatted(pDef.name.c_str());
					ImNodes::EndOutputAttribute();

					ImNodes::PopColorStyle();
				}

				ImNodes::EndNode();
			}

			// 2. Render all links
			for (const auto& link : m_graph.getLinks())
			{
				ImNodes::Link(link.id, link.startPinId, link.endPinId);
			}

			// MiniMap
			ImNodes::MiniMap(0.18f, ImNodesMiniMapLocation_BottomLeft);

			ImNodes::EndNodeEditor();

			// Save dragged node positions back to node.position
			for (auto& [nodeId, node] : m_graph.getNodes())
			{
				ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
				node.position = {pos.x, pos.y};
			}

			// 3. Link creation interaction
			int startAttr, endAttr;
			if (ImNodes::IsLinkCreated(&startAttr, &endAttr))
			{
				if (m_graph.addLink(startAttr, endAttr))
				{
					m_dirty = true;
				}
			}

			// 4. Link destruction interaction
			int linkId;
			if (ImNodes::IsLinkDestroyed(&linkId))
			{
				m_graph.removeLink(linkId);
				m_dirty = true;
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
						if (id != m_graph.getOutputNodeId())
						{
							m_graph.removeNode(id);
							m_dirty = true;
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
						m_graph.removeLink(id);
						m_dirty = true;
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
				renderAddNodeMenu(mousePos);
				ImGui::EndPopup();
			}

			// 7. Shortcut: Ctrl+L to trigger Auto Layout
			if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_L))
			{
				m_graph.autoLayout();
				m_syncPositionsToImNodes = true;
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
					openSaveFileDialog();
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_O))
				{
					openLoadFileDialog();
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
						ImGui::SetTooltip("Reset Zoom to 100% (Ctrl+0)");

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
						m_graph.autoLayout();
						m_syncPositionsToImNodes = true;
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

		void renderInspector(ServiceProvider& services)
		{
			if (ImGui::BeginTabBar("InspectorTabBar"))
			{
				// =============================================================
				// Tab 1: Procedural Music & Audio
				// =============================================================
				if (ImGui::BeginTabItem("Audio & Song"))
				{
					auto& music = services.audio().music();

					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.9f, 1.0f), "Procedural SDF Music Synthesizer");
					ImGui::Separator();

					if (ImGui::Checkbox("Play as Song (Real-Time)", &m_playSong))
					{
						music.setPlaying(m_playSong);
					}

					if (ImGui::SliderFloat("Master Volume", &m_musicVolume, 0.0f, 1.0f, "%.2f"))
					{
						music.setVolume(m_musicVolume);
					}

					ImGui::Spacing();
					if (ImGui::CollapsingHeader("Musical Properties", ImGuiTreeNodeFlags_DefaultOpen))
					{
						if (m_song)
						{
							ImGui::Text("Scale:  %s", getScaleName(m_song->getScale()));
							ImGui::Text("Tempo:  %.1f BPM", m_song->getTempo());
							ImGui::Text("Root:   %s (MIDI %d)", midiToNoteString(m_song->getRootMidi()).c_str(),
										m_song->getRootMidi());

							const auto& p = music.getShapeParameters();
							ImGui::Spacing();
							ImGui::Text("Melody Density:  %.2f", p.melodyDensity);
							ImGui::Text("Harmony Richness:%.2f", p.harmonyRichness);
							ImGui::Text("Bass Weight:     %.2f", p.bassWeight);
							ImGui::Text("Brightness:      %.2f", p.brightness);
							ImGui::Text("Syncopation:     %.2f", p.syncopation);

							ImGui::Spacing();
							ImGui::ProgressBar(music.getMotionNorm(), ImVec2(-1.0f, 0.0f), "Motion Level");
							ImGui::ProgressBar(music.getFillRatio(), ImVec2(-1.0f, 0.0f), "Domain Fill Ratio");
						}
					}

					ImGui::Spacing();
					if (ImGui::CollapsingHeader("Live Oscilloscope", ImGuiTreeNodeFlags_DefaultOpen))
					{
						auto audioData = WeirdRenderer::AudioEngine::getInstance().getAudioData();
						if (!audioData.waveform.empty())
						{
							ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 0.9f, 0.8f, 1.0f));
							ImGui::PlotLines("##Waveform", audioData.waveform.data(),
											 static_cast<int>(audioData.waveform.size()), 0, nullptr, -1.0f, 1.0f,
											 ImVec2(-1.0f, 75.0f));
							ImGui::PopStyleColor();
						}
						else
						{
							static const float zeroWave[128] = {0.0f};
							ImGui::PlotLines("##WaveformZero", zeroWave, 128, 0, "No Audio Signal", -1.0f, 1.0f,
											 ImVec2(-1.0f, 75.0f));
						}
					}

					ImGui::Spacing();
					if (ImGui::CollapsingHeader("Dynamic Feedback Triggers"))
					{
						if (ImGui::Button("Surge (+Impact)", ImVec2(160.0f, 25.0f)))
							music.surge(0.6f);
						ImGui::SameLine();
						if (ImGui::Button("Duck (-Volume)", ImVec2(160.0f, 25.0f)))
							music.duck(0.5f);

						if (ImGui::Button("Positive Trigger", ImVec2(160.0f, 25.0f)))
							music.triggerPositiveFeedback(1.0f);
						ImGui::SameLine();
						if (ImGui::Button("Negative Trigger", ImVec2(160.0f, 25.0f)))
							music.triggerNegativeFeedback(1.0f);

						if (ImGui::Button("Death Trigger", ImVec2(-1.0f, 25.0f)))
							music.triggerDeath();
					}

					ImGui::EndTabItem();
				}

				// =============================================================
				// Tab 2: Shader & Code Exporter
				// =============================================================
				if (ImGui::BeginTabItem("Code & Shader"))
				{
					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Live GLSL AST Shader Output");
					ImGui::Separator();

					if (ImGui::Button("Copy GLSL Code", ImVec2(-1.0f, 25.0f)))
					{
						ImGui::SetClipboardText(m_cachedGlslCode.c_str());
					}

					ImGui::InputTextMultiline("##GlslCodeBox", const_cast<char*>(m_cachedGlslCode.data()),
											  m_cachedGlslCode.size(), ImVec2(-1.0f, 150.0f),
											  ImGuiInputTextFlags_ReadOnly);

					ImGui::Spacing();
					ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "C++ Expr Code Snippet");
					ImGui::Separator();

					if (ImGui::Button("Copy C++ Code", ImVec2(-1.0f, 25.0f)))
					{
						ImGui::SetClipboardText(m_cachedCppCode.c_str());
					}

					ImGui::InputTextMultiline("##CppCodeBox", const_cast<char*>(m_cachedCppCode.data()),
											  m_cachedCppCode.size(), ImVec2(-1.0f, 200.0f),
											  ImGuiInputTextFlags_ReadOnly);

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}

		ImVec4 getCategoryColor(NodeCategory cat)
		{
			switch (cat)
			{
				case NodeCategory::Input:
					return ImVec4(0.95f, 0.75f, 0.3f, 1.0f);
				case NodeCategory::Vector:
					return ImVec4(0.95f, 0.55f, 0.2f, 1.0f);
				case NodeCategory::Transforms:
					return ImVec4(0.85f, 0.45f, 0.85f, 1.0f);
				case NodeCategory::Primitives2D:
					return ImVec4(0.35f, 0.85f, 0.95f, 1.0f);
				case NodeCategory::Primitives3D:
					return ImVec4(0.35f, 0.95f, 0.55f, 1.0f);
				case NodeCategory::CSG:
					return ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
				case NodeCategory::MathUnary:
				case NodeCategory::MathBinary:
				case NodeCategory::MathTernary:
					return ImVec4(0.55f, 0.75f, 0.95f, 1.0f);
				case NodeCategory::Output:
					return ImVec4(0.35f, 0.95f, 0.35f, 1.0f);
				default:
					return ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
			}
		}

		void openSaveFileDialog()
		{
			static const SDL_DialogFileFilter filters[] = {{"SDF Graph Files (*.json)", "json"},
														   {"All Files (*.*)", "*"}};
			SDL_ShowSaveFileDialog(onSaveFileCallback, this, nullptr, filters, 2, "sdf_graph.json");
		}

		void openLoadFileDialog()
		{
			static const SDL_DialogFileFilter filters[] = {{"SDF Graph Files (*.json)", "json"},
														   {"All Files (*.*)", "*"}};
			SDL_ShowOpenFileDialog(onOpenFileCallback, this, nullptr, filters, 2, nullptr, false);
		}

		static void SDLCALL onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
		{
			if (filelist && *filelist && **filelist)
			{
				auto* self = static_cast<SdfNodeEditorScene*>(userdata);
				std::lock_guard<std::mutex> lock(self->m_fileActionMutex);
				self->m_pendingLoadPath = *filelist;
			}
		}

		static void SDLCALL onSaveFileCallback(void* userdata, const char* const* filelist, int filter)
		{
			if (filelist && *filelist && **filelist)
			{
				auto* self = static_cast<SdfNodeEditorScene*>(userdata);
				std::lock_guard<std::mutex> lock(self->m_fileActionMutex);
				self->m_pendingSavePath = *filelist;
			}
		}

		void saveGraphToFile(std::string path)
		{
			if (path.length() < 5 || path.substr(path.length() - 5) != ".json")
			{
				path += ".json";
			}
			std::ofstream f(path);
			if (f.is_open())
			{
				f << m_graph.serialize().dump(4);
				f.close();
				Logger::log("Saved graph to " + path);
			}
			else
			{
				Logger::error("Failed to open file for saving: " + path);
			}
		}

		void loadGraphFromFile(const std::string& path)
		{
			std::ifstream f(path);
			if (f.is_open())
			{
				try
				{
					json j;
					f >> j;
					f.close();
					m_graph.deserialize(j);
					m_syncPositionsToImNodes = true;
					m_dirty = true;
					Logger::log("Loaded graph from " + path);
				}
				catch (const std::exception& e)
				{
					Logger::error(std::string("Failed to parse graph JSON: ") + e.what());
				}
			}
			else
			{
				Logger::error("Failed to open file for loading: " + path);
			}
		}

		std::mutex m_fileActionMutex;
		std::string m_pendingLoadPath;
		std::string m_pendingSavePath;

		NodeGraph m_graph;
		Expr m_evaluatedExpr;
		std::shared_ptr<WeirdRenderer::SdfSong> m_song;
		Entity m_previewEntity = INVALID_ENTITY;

		bool m_dirty = true;
		bool m_syncPositionsToImNodes = true;
		bool m_playSong = true;
		float m_musicVolume = 0.75f;

		std::string m_cachedGlslCode;
		std::string m_cachedCppCode;
	};
} // namespace WeirdEngine::Editor
