#pragma once

#include <weird-engine.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <SDL3/SDL_dialog.h>

#include "weird-engine/math/Default2DSDFs.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/GlobalPhysicsSettings.h"
#include "weird-physics/components/Spring.h"
#include <glm/gtx/norm.hpp>

extern WeirdEngine::vec3 g_cameraPositon;

using namespace WeirdEngine;

class MoleculeEditor : public Scene2D
{
public:
	MoleculeEditor() {}

	Registry* m_tempRegistry = nullptr;
	ServiceProvider* m_tempSvc = nullptr;

private:
	enum class DragMode
	{
		None,
		Ball,
		Camera
	};

	enum class ToolMode
	{
		Add,
		Spring,
		Distance,
		Modify,
		Remove,
		TagEditor,
		Material
	};

	enum class GridMode
	{
		Off,
		Square,
		Hex
	};

	enum class LinkType
	{
		Spring,
		Distance
	};

	struct BallInfo
	{
		Entity entity;
		int simulationId;
	};

	struct DistanceLink
	{
		Entity a;
		Entity b;
		int simulationIdA;
		int simulationIdB;
		float restDistance;
		Entity lineEntity;
		LinkType type;
		Entity constraintEntity;
	};

	std::vector<BallInfo> m_balls;
	std::vector<DistanceLink> m_links;

	Entity m_draggedBall = static_cast<Entity>(-1);
	int m_draggedSimulationId = -1;
	Entity m_constraintStartBall = static_cast<Entity>(-1);
	Entity m_draggedLinkConstraint = static_cast<Entity>(-1);
	float m_linkDragStartX = 0.0f;
	float m_linkDragStartDist = 0.0f;
	bool m_keepFixedAfterDrag = false;
	bool m_rightWasDown = false;
	bool m_middleWasDown = false;
	bool m_gravityEnabled = false;
	GridMode m_grid = GridMode::Off;
	DragMode m_dragMode = DragMode::None;
	int m_selectedMaterial = 1;

	ToolMode m_toolMode = ToolMode::Add;

	// ImGui editor UI state
	char m_fileNameBuf[128] = "molecule";
	char m_tagBuf[128] = {};
	Entity m_tagBufferEntity = static_cast<Entity>(-1);
	float m_menuBarHeight = 0.0f;
	bool m_cameraCentered = false;
	bool m_showControls = false;

	// Scene file state
	std::string m_currentFilePath;
	std::mutex m_fileMutex;
	std::string m_pendingLoadPath;
	std::string m_pendingSavePath;
	bool m_pendingNewMolecule = false;

	// Tag editor state
	Entity m_tagSelectedEntity = static_cast<Entity>(-1);
	Entity m_tagCircleOuter = static_cast<Entity>(-1);
	Entity m_tagCircleInner = static_cast<Entity>(-1);

	static constexpr float BALL_HIT_RADIUS = 0.9f;
	static constexpr float LINE_WIDTH = 3.5f;
	static constexpr float MODIFY_LINE_WIDTH = 8.0f;
	static constexpr float LINK_PICK_PIXELS = 14.0f;
	static constexpr float SPRING_STIFFNESS = 0.15f;
	static constexpr float CONSTRAINT_STIFFNESS = 0.95f;
	static constexpr float GRID_CELL = 1.0f;
	static constexpr float HEX_ROW_HEIGHT = GRID_CELL * 0.86602540378f;
	static constexpr float CAMERA_MIN_DISTANCE = 5.0f;
	static constexpr float CAMERA_MAX_DISTANCE = 300.0f;
	static constexpr float CAMERA_ZOOM_STEP = 0.12f;
	static constexpr float TAG_OUTER_RADIUS = 30.0f;
	static constexpr float TAG_INNER_RADIUS = 25.0f;
	static constexpr int TAG_RING_GROUP = 8;
	static constexpr float PANEL_WIDTH = 300.0f;

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		m_tempRegistry = &registry;
		m_tempSvc = &services;

		g_cameraPositon.x = 0.0f;
		g_cameraPositon.y = 0.0f;
		m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity()).position = g_cameraPositon;

		// Request neutral simulation behavior for this editor scene.
		Entity globalSettingsEnt = m_tempRegistry->createEntity();
		auto& settings = m_tempRegistry->addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = 0.0f;
		settings.damping = 1.0f;
		m_tempRegistry->setComponentDirty(settings);

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = m_tempSvc->materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		{
			Entity outside = m_tempSvc->shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
														   .variables = {0.0f, 0.0f, 3000.0f},
														   .material = 0,
														   .combination = CombinationType::Addition});

			Entity inside = m_tempSvc->shapes().addShape({.shapeId = DefaultShapes::BOX,
														  .variables = {0.0f, 0.0f, 20.0f, 20.0f},
														  .material = 0,
														  .combination = CombinationType::Subtraction});

			m_tempSvc->serialization().blacklistEntity(outside);
			m_tempSvc->serialization().blacklistEntity(inside);
		}
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		m_tempRegistry = &registry;
		m_tempSvc = &services;
		g_cameraPositon = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity()).position;

		if (m_tempSvc->input().getKeyDown(Input::Q) ||
			m_tempSvc->input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			m_tempSvc->sceneControl().goToNextScene();
			return;
		}

		if (m_tempSvc->input().getKey(Input::LeftCtrl) && m_tempSvc->input().getKeyDown(Input::S))
		{
			saveMolecule();
		}

		if (m_tempSvc->input().getKey(Input::LeftCtrl) && m_tempSvc->input().getKeyDown(Input::O))
		{
			openLoadFileDialog();
		}

		if (m_tempSvc->input().getKey(Input::LeftCtrl) && m_tempSvc->input().getKeyDown(Input::L))
		{
			openLoadFileDialog();
		}

		if (m_tempSvc->input().getKey(Input::LeftCtrl) && m_tempSvc->input().getKeyDown(Input::N))
		{
			m_pendingNewMolecule = true;
		}

		pollFileActions();

		if (m_pendingNewMolecule)
		{
			m_pendingNewMolecule = false;
			clearMolecule();
		}

		handleCameraInput();
		handleRightMouseInput();
		handleLeftClickInput();
		handleLinkInteraction();
		updateConstraintLines();
		updateTagEditor();
		removeFallenBalls();
	}

	void onCustomUI(Registry& registry, ServiceProvider& services) override
	{
		m_tempRegistry = &registry;
		m_tempSvc = &services;

		renderMenuBar();
		renderSidePanel();
		renderControlsWindow();
		renderSceneOverlay();

		if (!m_cameraCentered && m_menuBarHeight > 0.0f)
		{
			m_cameraCentered = true;
			centerCameraOnSceneView();
		}
	}

	// -----------------------------------------------------------------------
	// ImGui editor UI
	// -----------------------------------------------------------------------

	void renderMenuBar()
	{
		ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		if (ImGui::BeginMainMenuBar())
		{
			m_menuBarHeight = ImGui::GetWindowSize().y;

			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Molecule", "Ctrl+N"))
				{
					m_pendingNewMolecule = true;
				}
				if (ImGui::MenuItem("Open Molecule...", "Ctrl+O"))
				{
					openLoadFileDialog();
				}

				const bool hasFile = !m_currentFilePath.empty();
				if (ImGui::MenuItem(hasFile ? "Save Molecule" : "Save Molecule...", "Ctrl+S"))
				{
					saveMolecule();
				}
				if (ImGui::MenuItem("Save Molecule As..."))
				{
					openSaveFileDialog();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Exit Tool", "Q"))
				{
					m_tempSvc->sceneControl().goToNextScene();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Reset Camera"))
				{
					centerCameraOnSceneView();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				ImGui::MenuItem("Controls", nullptr, &m_showControls);
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
		ImGui::PopStyleColor();
	}

	void renderToolsSection()
	{
		if (!ImGui::CollapsingHeader("Tools", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const char* labels[] = {"Add", "Spring", "Distance", "Modify Link", "Remove", "Tag Editor", "Material"};
		const ToolMode modes[] = {ToolMode::Add,	 ToolMode::Spring, ToolMode::Distance, ToolMode::Modify,
								  ToolMode::Remove, ToolMode::TagEditor, ToolMode::Material};

		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float buttonWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

		for (int i = 0; i < 7; ++i)
		{
			if (i % 2 != 0)
			{
				ImGui::SameLine();
			}

			const bool selected = (m_toolMode == modes[i]);
			ImGui::PushID(i);
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.65f, 1.0f));
			}
			if (ImGui::Button(labels[i], ImVec2(buttonWidth, 26.0f)))
			{
				m_toolMode = modes[i];
				m_constraintStartBall = static_cast<Entity>(-1);
			}
			if (selected)
			{
				ImGui::PopStyleColor();
			}
			ImGui::PopID();
		}

		ImGui::TextDisabled("Left click applies the active tool.");
	}

	void renderOptionsSection()
	{
		if (!ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		if (ImGui::Checkbox("Gravity", &m_gravityEnabled))
		{
			applyGravitySettings();
		}

		const char* gridItems[] = {"Off", "Square", "Hex"};
		int gridIndex = static_cast<int>(m_grid);
		if (ImGui::Combo("Grid Snap", &gridIndex, gridItems, IM_ARRAYSIZE(gridItems)))
		{
			m_grid = static_cast<GridMode>(gridIndex);
		}
	}

	void renderMoleculesSection()
	{
		if (!ImGui::CollapsingHeader("Molecules", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float buttonWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

		if (ImGui::Button("Center & Relax", ImVec2(buttonWidth, 26.0f)))
		{
			centerAndRelaxMolecules();
		}

		ImGui::SameLine();

		const bool gridOn = gridEnabled();
		if (!gridOn)
		{
			ImGui::BeginDisabled();
		}
		const bool fitToGrid = ImGui::Button("Fit to Grid", ImVec2(buttonWidth, 26.0f));
		if (!gridOn)
		{
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("Pick a grid shape in Options first.");
			}
		}

		if (fitToGrid)
		{
			fitMoleculesToGrid();
		}
	}

	void renderSidePanel()
	{
		int winW = Display::width > 0 ? Display::width : 1280;
		int winH = Display::height > 0 ? Display::height : 800;

		const float panelH = (std::max)(0.0f, static_cast<float>(winH) - m_menuBarHeight);

		ImGui::SetNextWindowPos(ImVec2(static_cast<float>(winW) - PANEL_WIDTH, m_menuBarHeight), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(PANEL_WIDTH, panelH), ImGuiCond_Always);

		const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
									   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
									   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNavFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.115f, 0.115f, 0.125f, 1.0f));

		if (ImGui::Begin("##MoleculePanel", nullptr, flags))
		{
			renderToolsSection();
			ImGui::Spacing();
			renderOptionsSection();
			ImGui::Spacing();
			renderMoleculesSection();
			ImGui::Spacing();
			renderMaterialsSection();
			ImGui::Spacing();
			renderTagSection();
			ImGui::Spacing();
			renderLinksSection();
			ImGui::Spacing();
			renderStatsSection();
#ifdef __EMSCRIPTEN__
			ImGui::Spacing();
			renderWebFileSection();
#endif
		}
		ImGui::End();

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);

		// Frame the interactive scene view on the left so the layout is obvious.
		ImGui::GetForegroundDrawList()->AddRect(
			ImVec2(1.0f, m_menuBarHeight + 1.0f),
			ImVec2(static_cast<float>(winW) - PANEL_WIDTH - 1.0f, static_cast<float>(winH) - 1.0f),
			IM_COL32(60, 60, 66, 255));
	}

	void renderControlsWindow()
	{
		if (!m_showControls)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(400.0f, 360.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(40.0f, m_menuBarHeight + 40.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Molecule Editor - Controls", &m_showControls))
		{
			ImGui::TextWrapped("Left click: apply the active tool.");
			ImGui::BulletText("Add: spawn a ball with the selected material");
			ImGui::BulletText("Spring / Distance: click two balls to connect them");
			ImGui::BulletText("Modify Link: links get thick; drag one sideways to change its distance");
			ImGui::BulletText("Remove: click two connected balls, or click a link to delete it");
			ImGui::BulletText("Tag Editor: click a ball, then edit its tag in the panel");
			ImGui::BulletText("Material: click a ball to repaint it");
			ImGui::Separator();
			ImGui::TextWrapped("Right click always drags: hold it over a ball to move it (F pins it in "
							   "place), or over empty space to pan the camera.");
			ImGui::TextWrapped("Middle mouse drag pans the camera, mouse wheel zooms.");
			ImGui::Separator();
			ImGui::TextWrapped("With grid snapping on, moved atoms (drag or Fit to Grid) resize their links to "
							   "the new distance.");
			ImGui::Separator();
			ImGui::BulletText("Ctrl+N: new molecule");
			ImGui::BulletText("Ctrl+O / Ctrl+L: open a .weird scene");
			ImGui::BulletText("Ctrl+S: save (asks for a path the first time)");
			ImGui::BulletText("Q: exit the tool");
		}
		ImGui::End();
	}

	void renderSceneOverlay()
	{
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->PushClipRect(ImVec2(0.0f, m_menuBarHeight),
							   ImVec2(static_cast<float>(Display::width) - PANEL_WIDTH,
									  static_cast<float>(Display::height)),
							   true);
		renderGridOverlay();
		renderPendingConstraintOverlay();
		drawList->PopClipRect();
	}

	void renderGridOverlay()
	{
		if (!gridEnabled())
		{
			return;
		}

		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float winW = static_cast<float>((std::max)(1, Display::width));
		const float winH = static_cast<float>((std::max)(1, Display::height));
		const float worldPerPixel = cam.position.z / (winH * 0.5f);
		if (GRID_CELL / worldPerPixel < 5.0f)
		{
			return; // too dense to be legible
		}

		const vec2 cornerA = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(0.0f, 0.0f));
		const vec2 cornerB = ECS::Camera::screenPositionToWorldPosition2D(cam, vec2(winW, winH));
		const float margin = GRID_CELL * 2.0f;
		const float minX = (std::min)(cornerA.x, cornerB.x) - margin;
		const float maxX = (std::max)(cornerA.x, cornerB.x) + margin;
		const float minY = (std::min)(cornerA.y, cornerB.y) - margin;
		const float maxY = (std::max)(cornerA.y, cornerB.y) + margin;

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const ImU32 color = IM_COL32(115, 135, 170, 60);

		auto toScreen = [&](vec2 world) -> ImVec2
		{
			const vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, world);
			return ImVec2(screen.x, winH - screen.y);
		};

		if (m_grid == GridMode::Square)
		{
			for (int x = static_cast<int>(std::floor(minX / GRID_CELL));
				 x <= static_cast<int>(std::ceil(maxX / GRID_CELL)); ++x)
			{
				const float wx = x * GRID_CELL;
				drawList->AddLine(toScreen(vec2(wx, minY)), toScreen(vec2(wx, maxY)), color);
			}
			for (int y = static_cast<int>(std::floor(minY / GRID_CELL));
				 y <= static_cast<int>(std::ceil(maxY / GRID_CELL)); ++y)
			{
				const float wy = y * GRID_CELL;
				drawList->AddLine(toScreen(vec2(minX, wy)), toScreen(vec2(maxX, wy)), color);
			}
			return;
		}

		// Hex grid: the centers form a triangular lattice. Draw its three
		// families of parallel lines so the hex arrangement is visible.
		const float h = HEX_ROW_HEIGHT;
		const float s3 = 0.86602540378f;
		const float diagonal = glm::length(vec2(maxX - minX, maxY - minY));
		const vec2 dirB(0.5f, s3);
		const vec2 dirC(0.5f, -s3);
		const vec2 normalB(-s3, 0.5f);
		const vec2 normalC(s3, 0.5f);

		// Family A: horizontal lines.
		for (int k = static_cast<int>(std::floor(minY / h)); k <= static_cast<int>(std::ceil(maxY / h)); ++k)
		{
			const float wy = k * h;
			drawList->AddLine(toScreen(vec2(minX, wy)), toScreen(vec2(maxX, wy)), color);
		}

		auto drawDiagonalFamily = [&](const vec2& normal, const vec2& direction)
		{
			const float c0 = (std::min)((std::min)(normal.x * minX + normal.y * minY, normal.x * maxX + normal.y * minY),
										(std::min)(normal.x * minX + normal.y * maxY, normal.x * maxX + normal.y * maxY));
			const float c1 = (std::max)((std::max)(normal.x * minX + normal.y * minY, normal.x * maxX + normal.y * minY),
										(std::max)(normal.x * minX + normal.y * maxY, normal.x * maxX + normal.y * maxY));
			for (int k = static_cast<int>(std::floor(c0 / h)); k <= static_cast<int>(std::ceil(c1 / h)); ++k)
			{
				const vec2 center = normal * (k * h);
				drawList->AddLine(toScreen(center - direction * diagonal), toScreen(center + direction * diagonal),
								  color);
			}
		};

		drawDiagonalFamily(normalB, dirB);
		drawDiagonalFamily(normalC, dirC);
	}

	void renderPendingConstraintOverlay()
	{
		if (m_constraintStartBall == static_cast<Entity>(-1) || !hasTransform(m_constraintStartBall))
		{
			return;
		}

		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const auto& t = m_tempRegistry->getComponent<Transform>(m_constraintStartBall);
		const vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, vec2(t.position.x, t.position.y));
		const ImVec2 center(screen.x, static_cast<float>(Display::height) - screen.y);

		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		drawList->AddCircle(center, TAG_OUTER_RADIUS, IM_COL32(120, 220, 150, 220), 0, 2.5f);
		drawList->AddLine(center, ImGui::GetIO().MousePos, IM_COL32(120, 220, 150, 110), 2.0f);
	}

	void renderMaterialsSection()
	{
		if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float swatch = 26.0f;
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float available = ImGui::GetContentRegionAvail().x;
		const int columns = (std::max)(1, static_cast<int>((available + spacing) / (swatch + spacing)));

		for (int i = 0; i < 16; ++i)
		{
			if (i % columns != 0)
			{
				ImGui::SameLine();
			}

			ImGui::PushID(i);

			const bool selected = (m_selectedMaterial == i);
			const vec4 color = m_tempSvc->materials2D().get(static_cast<uint16_t>(i)).color;

			if (selected)
			{
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.5f);
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			}

			if (ImGui::ColorButton("##swatch", ImVec4(color.x, color.y, color.z, 1.0f),
								   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
								   ImVec2(swatch, swatch)))
			{
				m_selectedMaterial = i;
			}

			if (selected)
			{
				ImGui::PopStyleColor();
				ImGui::PopStyleVar();
			}

			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Material %d%s", i, selected ? " (selected)" : "");
			}

			ImGui::PopID();
		}

		ImGui::TextDisabled("Selected: material %d", m_selectedMaterial);
	}

	void renderTagSection()
	{
		if (!ImGui::CollapsingHeader("Tag Editor", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		if (m_tagSelectedEntity == static_cast<Entity>(-1) || !hasTransform(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
			ImGui::TextWrapped("No molecule selected. Pick the Tag tool and right-click a ball to select it.");
			return;
		}

		if (m_tagBufferEntity != m_tagSelectedEntity)
		{
			m_tagBufferEntity = m_tagSelectedEntity;
			const std::string currentTag = m_tempSvc->tags().getEntityTag(m_tagSelectedEntity);
			std::snprintf(m_tagBuf, sizeof(m_tagBuf), "%s", currentTag.c_str());
		}

		ImGui::Text("Ball: entity %u", static_cast<unsigned int>(m_tagSelectedEntity));
		ImGui::SetNextItemWidth(-1.0f);
		const bool submitted =
			ImGui::InputTextWithHint("##tag", "tag name", m_tagBuf, sizeof(m_tagBuf),
									 ImGuiInputTextFlags_EnterReturnsTrue);

		const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		const bool apply = ImGui::Button("Apply", ImVec2(buttonWidth, 0.0f)) || submitted;
		ImGui::SameLine();
		const bool remove = ImGui::Button("Remove", ImVec2(buttonWidth, 0.0f));

		if (apply)
		{
			const std::string newTag(m_tagBuf);
			if (newTag.empty())
			{
				m_tempSvc->tags().removeTag(m_tagSelectedEntity);
			}
			else
			{
				m_tempSvc->tags().tag(m_tagSelectedEntity, newTag);
			}
		}

		if (remove)
		{
			m_tempSvc->tags().removeTag(m_tagSelectedEntity);
			m_tagBuf[0] = '\0';
		}

		if (ImGui::Button("Deselect", ImVec2(-1.0f, 0.0f)))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
			m_tagBufferEntity = static_cast<Entity>(-1);
		}
	}

	void renderLinksSection()
	{
		if (!ImGui::CollapsingHeader("Links", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		ImGui::TextDisabled("%zu constraint links", m_links.size());
		if (m_links.empty())
		{
			return;
		}

		int removeIndex = -1;

		ImGui::BeginChild("##LinksList", ImVec2(0.0f, 150.0f), true);
		for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
		{
			DistanceLink& link = m_links[i];
			ImGui::PushID(i);

			const char* typeName = (link.type == LinkType::Distance) ? "Distance" : "Spring";
			ImGui::Text("%s  #%d <-> #%d", typeName, link.simulationIdA, link.simulationIdB);

			float distance = link.restDistance;
			ImGui::SetNextItemWidth(-34.0f);
			if (ImGui::SliderFloat("##distance", &distance, 1.0f, 10.0f, "%.1f"))
			{
				link.restDistance = distance;
				applyLinkDistance(link);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("X"))
			{
				removeIndex = i;
			}

			ImGui::PopID();
		}
		ImGui::EndChild();

		if (removeIndex >= 0)
		{
			removeLinkAt(static_cast<size_t>(removeIndex));
		}
	}

	void renderWebFileSection()
	{
		if (!ImGui::CollapsingHeader("Scene File", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		ImGui::TextDisabled("Saved under assets/Organisms/");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##filename", "molecule.weird", m_fileNameBuf, sizeof(m_fileNameBuf));

		const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
		{
			saveMoleculeFromName();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load", ImVec2(buttonWidth, 0.0f)))
		{
			loadMoleculeFromName();
		}
	}

	void renderStatsSection()
	{
		if (!ImGui::CollapsingHeader("Stats"))
		{
			return;
		}

		ImGui::Text("Balls: %zu   Links: %zu", m_balls.size(), m_links.size());
		ImGui::Text("Gravity: %s", m_gravityEnabled ? "on" : "off");
		ImGui::Text("Grid: %s", gridModeName());
		ImGui::TextDisabled("File: %s", m_currentFilePath.empty() ? "(unsaved)" : m_currentFilePath.c_str());
	}

	const char* gridModeName() const
	{
		switch (m_grid)
		{
			case GridMode::Square:
				return "Square";
			case GridMode::Hex:
				return "Hex";
			case GridMode::Off:
			default:
				return "Off";
		}
	}

	void centerCameraOnSceneView()
	{
		int winW = Display::width > 0 ? Display::width : 1280;
		int winH = Display::height > 0 ? Display::height : 800;

		auto& camTransform = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float halfH = static_cast<float>(winH) * 0.5f;
		const float viewCenterX = (static_cast<float>(winW) - PANEL_WIDTH) * 0.5f;
		const float viewCenterY = m_menuBarHeight + (static_cast<float>(winH) - m_menuBarHeight) * 0.5f;
		const float scale = camTransform.position.z / halfH;

		camTransform.position.x = (static_cast<float>(winW) * 0.5f - viewCenterX) * scale;
		camTransform.position.y = (halfH - viewCenterY) * scale;
		m_tempRegistry->setComponentDirty(camTransform);
		g_cameraPositon = camTransform.position;
	}

	void centerAndRelaxMolecules()
	{
		if (m_balls.empty())
		{
			return;
		}

		vec2 centroid(0.0f);
		size_t ballCount = 0;
		for (const BallInfo& ball : m_balls)
		{
			if (!hasTransform(ball.entity))
			{
				continue;
			}

			const auto& t = m_tempRegistry->getComponent<Transform>(ball.entity);
			centroid += vec2(t.position.x, t.position.y);
			++ballCount;
		}

		if (ballCount == 0)
		{
			return;
		}
		centroid /= static_cast<float>(ballCount);

		for (const BallInfo& ball : m_balls)
		{
			if (!hasTransform(ball.entity))
			{
				continue;
			}

			auto& t = m_tempRegistry->getComponent<Transform>(ball.entity);
			t.position.x -= centroid.x;
			t.position.y -= centroid.y;
			m_tempRegistry->setComponentDirty(t);

			if (m_tempRegistry->hasComponent<RigidBody2D>(ball.entity))
			{
				auto& rb = m_tempRegistry->getComponent<RigidBody2D>(ball.entity);
				rb.velocity = vec2(0.0f);
				m_tempRegistry->setComponentDirty(rb);
			}
		}

		if (gridEnabled())
		{
			// Keep the centered structure on the grid when snapping is enabled.
			for (const BallInfo& ball : m_balls)
			{
				snapBallToGrid(ball);
			}
		}

		// Relax every link to its current length so the molecule holds its shape.
		for (DistanceLink& link : m_links)
		{
			resizeLinkToCurrentDistance(link);
		}
	}

	void fitMoleculesToGrid()
	{
		if (!gridEnabled())
		{
			return;
		}

		for (const BallInfo& ball : m_balls)
		{
			snapBallToGrid(ball);
		}

		// Snap positions changed the geometry: resize every link to match it.
		for (DistanceLink& link : m_links)
		{
			resizeLinkToCurrentDistance(link);
		}
	}

	void snapBallToGrid(const BallInfo& ball)
	{
		if (!hasTransform(ball.entity))
		{
			return;
		}

		auto& t = m_tempRegistry->getComponent<Transform>(ball.entity);
		const vec2 snapped = snapToGrid(vec2(t.position.x, t.position.y), ball.entity);
		t.position.x = snapped.x;
		t.position.y = snapped.y;
		m_tempRegistry->setComponentDirty(t);

		if (m_tempRegistry->hasComponent<RigidBody2D>(ball.entity))
		{
			auto& rb = m_tempRegistry->getComponent<RigidBody2D>(ball.entity);
			rb.velocity = vec2(0.0f);
			m_tempRegistry->setComponentDirty(rb);
		}
	}

	void applyGravitySettings()
	{
		auto globalSettingsArray = m_tempRegistry->getComponentArray<GlobalPhysicsSettings>();
		if (globalSettingsArray->getSize() == 0)
		{
			return;
		}

		auto& settings = globalSettingsArray->getDataAtIdx(0);
		if (m_gravityEnabled)
		{
			settings.gravity = -10.0f;
			settings.damping = 0.01f;
		}
		else
		{
			settings.gravity = 0.0f;
			settings.damping = 1.0f;
		}
		m_tempRegistry->setComponentDirty(settings);
	}

	void applyLinkDistance(DistanceLink& link)
	{
		if (link.type == LinkType::Distance)
		{
			if (m_tempRegistry->hasComponent<DistanceConstraint>(link.constraintEntity))
			{
				auto& constraint = m_tempRegistry->getComponent<DistanceConstraint>(link.constraintEntity);
				constraint.distance = link.restDistance;
				m_tempRegistry->setComponentDirty(constraint);
			}
		}
		else
		{
			if (m_tempRegistry->hasComponent<Spring>(link.constraintEntity))
			{
				auto& spring = m_tempRegistry->getComponent<Spring>(link.constraintEntity);
				spring.restDistance = link.restDistance;
				m_tempRegistry->setComponentDirty(spring);
			}
		}
	}

	void resizeLinkToCurrentDistance(DistanceLink& link)
	{
		if (!hasTransform(link.a) || !hasTransform(link.b))
		{
			return;
		}

		const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
		const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
		const vec2 pa(ta.position.x, ta.position.y);
		const vec2 pb(tb.position.x, tb.position.y);

		// Auto-resize has no upper bound: the link follows wherever the atom
		// was snapped to. The line-drag UI still clamps to its slider range.
		link.restDistance = (std::max)(glm::length(pb - pa), 1.0f);
		applyLinkDistance(link);
	}

	void resizeLinksForBall(Entity ball)
	{
		for (DistanceLink& link : m_links)
		{
			if (link.a == ball || link.b == ball)
			{
				resizeLinkToCurrentDistance(link);
			}
		}
	}

	void removeLinkAt(size_t index)
	{
		if (index >= m_links.size())
		{
			return;
		}

		DistanceLink& link = m_links[index];
		if (m_draggedLinkConstraint == link.constraintEntity)
		{
			m_draggedLinkConstraint = static_cast<Entity>(-1);
		}

		m_tempRegistry->destroyEntity(link.constraintEntity);
		m_tempRegistry->destroyEntity(link.lineEntity);
		m_links.erase(m_links.begin() + static_cast<std::ptrdiff_t>(index));
	}

	// -----------------------------------------------------------------------
	// Scene files
	// -----------------------------------------------------------------------

	void clearMolecule()
	{
		for (DistanceLink& link : m_links)
		{
			m_tempRegistry->destroyEntity(link.constraintEntity);
			m_tempRegistry->destroyEntity(link.lineEntity);
		}
		m_links.clear();

		for (const BallInfo& ball : m_balls)
		{
			m_tempSvc->tags().removeTag(ball.entity);
			m_tempRegistry->destroyEntity(ball.entity);
		}
		m_balls.clear();

		m_draggedBall = static_cast<Entity>(-1);
		m_draggedSimulationId = -1;
		m_draggedLinkConstraint = static_cast<Entity>(-1);
		m_constraintStartBall = static_cast<Entity>(-1);
		m_keepFixedAfterDrag = false;
		m_dragMode = DragMode::None;
		m_rightWasDown = false;
		m_tagSelectedEntity = static_cast<Entity>(-1);
		m_currentFilePath.clear();
	}

	void saveMolecule()
	{
		if (m_currentFilePath.empty())
		{
			openSaveFileDialog();
			return;
		}
		saveMoleculeTo(m_currentFilePath);
	}

	void saveMoleculeTo(const std::string& path)
	{
		std::string finalPath = path;
		if (!finalPath.ends_with(".weird"))
		{
			finalPath += ".weird";
		}

		m_tempSvc->serialization().saveScene(finalPath);
		m_currentFilePath = finalPath;
		WeirdEngine::Logger::log("Saved molecule to " + finalPath);
	}

	void openLoadFileDialog()
	{
#ifdef __EMSCRIPTEN__
		loadMoleculeFromName();
#else
		static SDL_DialogFileFilter filters[1] = {{"Weird Molecule (*.weird)", "weird"}};
		SDL_ShowOpenFileDialog(onOpenFileCallback, this, nullptr, filters, 1, nullptr, false);
#endif
	}

	void openSaveFileDialog()
	{
#ifdef __EMSCRIPTEN__
		saveMoleculeFromName();
#else
		static SDL_DialogFileFilter filters[1] = {{"Weird Molecule (*.weird)", "weird"}};
		SDL_ShowSaveFileDialog(onSaveFileCallback, this, nullptr, filters, 1, "molecule.weird");
#endif
	}

	void pollFileActions()
	{
		std::string loadPath;
		std::string savePath;
		{
			std::lock_guard<std::mutex> lock(m_fileMutex);
			loadPath = std::exchange(m_pendingLoadPath, {});
			savePath = std::exchange(m_pendingSavePath, {});
		}

		if (!loadPath.empty())
		{
			loadMolecule(loadPath);
			m_currentFilePath = loadPath;
		}

		if (!savePath.empty())
		{
			saveMoleculeTo(savePath);
		}
	}

#ifndef __EMSCRIPTEN__
	static void SDLCALL onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* editor = static_cast<MoleculeEditor*>(userdata);
		if (!editor || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(editor->m_fileMutex);
		editor->m_pendingLoadPath = *filelist;
	}

	static void SDLCALL onSaveFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* editor = static_cast<MoleculeEditor*>(userdata);
		if (!editor || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(editor->m_fileMutex);
		editor->m_pendingSavePath = *filelist;
	}
#endif

	std::string currentFileName() const
	{
		std::string fileName(m_fileNameBuf);
		if (fileName.empty())
		{
			fileName = "molecule";
		}
		if (!fileName.ends_with(".weird"))
		{
			fileName += ".weird";
		}
		return fileName;
	}

	void saveMoleculeFromName()
	{
		saveMoleculeTo(m_tempSvc->resources().assetPath("Organisms/") + currentFileName());
	}

	void loadMoleculeFromName()
	{
		// Only used by the Emscripten file fallback; keep it referenced everywhere
		// to avoid dead-code surprises when building for the web.
		const std::string path = m_tempSvc->resources().assetPath("Organisms/") + currentFileName();
		loadMolecule(path);
		m_currentFilePath = path;
	}

	void removeFallenBalls()
	{
		std::vector<Entity> toDelete;
		toDelete.reserve(m_balls.size());

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
			{
				toDelete.push_back(b.entity);
				continue;
			}

			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			if (t.position.y < -1000.0f)
			{
				toDelete.push_back(b.entity);
			}
		}

		if (toDelete.empty())
			return;

		auto shouldDelete = [&toDelete](Entity e)
		{ return std::find(toDelete.begin(), toDelete.end(), e) != toDelete.end(); };

		for (Entity e : toDelete)
		{
			m_tempRegistry->destroyEntity(e);
		}

		m_links.erase(std::remove_if(m_links.begin(), m_links.end(),
									 [&](const DistanceLink& link)
									 {
										 bool remove = shouldDelete(link.a) || shouldDelete(link.b);
										 if (remove)
										 {
											 if (m_draggedLinkConstraint == link.constraintEntity)
												 m_draggedLinkConstraint = static_cast<Entity>(-1);
											 m_tempRegistry->destroyEntity(link.lineEntity);
										 }
										 return remove;
									 }),
					  m_links.end());

		m_balls.erase(
			std::remove_if(m_balls.begin(), m_balls.end(), [&](const BallInfo& b) { return shouldDelete(b.entity); }),
			m_balls.end());

		if (shouldDelete(m_draggedBall))
		{
			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = -1;
			m_keepFixedAfterDrag = false;
			m_dragMode = DragMode::None;
			m_rightWasDown = false;
		}

		if (shouldDelete(m_constraintStartBall))
		{
			m_constraintStartBall = static_cast<Entity>(-1);
		}

		if (shouldDelete(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
		}
	}

	void spawnBallAtMouse()
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		vec2 world = ECS::Camera::screenPositionToWorldPosition2D(
			cam, vec2(m_tempSvc->input().getMouseX(), m_tempSvc->input().getMouseY()));

		if (gridEnabled())
			world = snapToGrid(world);

		Entity e = m_tempRegistry->createEntity();

		auto& t = m_tempRegistry->addComponent<Transform>(e);
		t.position = vec3(world.x, world.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		auto& sdf = m_tempRegistry->addComponent<Dot>(e);
		sdf.materialId = static_cast<unsigned int>(m_selectedMaterial);

		auto& rb = m_tempRegistry->addComponent<RigidBody2D>(e);

		m_balls.push_back({e, static_cast<int>(rb.simulationId)});
	}

	vec2 getMouseWorldPosition()
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		return ECS::Camera::screenPositionToWorldPosition2D(
			cam, vec2(m_tempSvc->input().getMouseX(), m_tempSvc->input().getMouseY()));
	}

	bool gridEnabled() const
	{
		return m_grid != GridMode::Off;
	}

	vec2 snapToGrid(vec2 pos, Entity exclude = static_cast<Entity>(-1))
	{
		if (m_grid == GridMode::Hex)
		{
			return snapToHexGrid(pos, exclude);
		}
		return snapToSquareGrid(pos, exclude);
	}

	vec2 snapToSquareGrid(vec2 pos, Entity exclude)
	{
		vec2 nearest(std::round(pos.x / GRID_CELL) * GRID_CELL, std::round(pos.y / GRID_CELL) * GRID_CELL);

		if (!isCellOccupied(nearest, exclude))
			return nearest;

		// Spiral outward to find the closest free cell.
		for (int radius = 1; radius <= 50; ++radius)
		{
			float bestDist = (std::numeric_limits<float>::max)();
			vec2 bestCell = nearest;
			bool found = false;

			for (int dx = -radius; dx <= radius; ++dx)
			{
				for (int dy = -radius; dy <= radius; ++dy)
				{
					if (std::abs(dx) != radius && std::abs(dy) != radius)
						continue; // only check the outer ring

					vec2 candidate(nearest.x + dx * GRID_CELL, nearest.y + dy * GRID_CELL);

					if (isCellOccupied(candidate, exclude))
						continue;

					float d = glm::length(candidate - pos);
					if (d < bestDist)
					{
						bestDist = d;
						bestCell = candidate;
						found = true;
					}
				}
			}

			if (found)
				return bestCell;
		}

		return nearest; // fallback
	}

	// Hex grid centers use an odd-r offset layout: rows are spaced by
	// sqrt(3)/2 cells and every other row is offset by half a cell.
	vec2 hexCellCenter(int col, int row) const
	{
		const float offset = ((row & 1) != 0) ? GRID_CELL * 0.5f : 0.0f;
		return vec2(col * GRID_CELL + offset, row * HEX_ROW_HEIGHT);
	}

	vec2 nearestHexCellCenter(vec2 pos) const
	{
		const int baseRow = static_cast<int>(std::round(pos.y / HEX_ROW_HEIGHT));
		vec2 best = hexCellCenter(0, 0);
		float bestDist = (std::numeric_limits<float>::max)();

		for (int row = baseRow - 1; row <= baseRow + 1; ++row)
		{
			const float offset = ((row & 1) != 0) ? GRID_CELL * 0.5f : 0.0f;
			const int col = static_cast<int>(std::round((pos.x - offset) / GRID_CELL));
			const vec2 center = hexCellCenter(col, row);
			const float d = glm::length2(center - pos);
			if (d < bestDist)
			{
				bestDist = d;
				best = center;
			}
		}

		return best;
	}

	vec2 snapToHexGrid(vec2 pos, Entity exclude)
	{
		const vec2 nearest = nearestHexCellCenter(pos);
		if (!isCellOccupied(nearest, exclude))
			return nearest;

		// Spiral through hex rings in axial coordinates to find the closest free cell.
		const int centerRow = static_cast<int>(std::round(nearest.y / HEX_ROW_HEIGHT));
		const int centerCol = static_cast<int>(std::round(nearest.x / GRID_CELL));
		const int centerQ = centerCol - (centerRow - (centerRow & 1)) / 2;
		const int centerR = centerRow;

		for (int radius = 1; radius <= 50; ++radius)
		{
			float bestDist = (std::numeric_limits<float>::max)();
			vec2 bestCell = nearest;
			bool found = false;

			for (int q = centerQ - radius; q <= centerQ + radius; ++q)
			{
				for (int r = centerR - radius; r <= centerR + radius; ++r)
				{
					const int dq = q - centerQ;
					const int dr = r - centerR;
					if ((std::max)({std::abs(dq), std::abs(dr), std::abs(dq + dr)}) != radius)
						continue; // only check the outer ring

					const int col = q + (r - (r & 1)) / 2;
					const vec2 candidate = hexCellCenter(col, r);

					if (isCellOccupied(candidate, exclude))
						continue;

					const float d = glm::length(candidate - pos);
					if (d < bestDist)
					{
						bestDist = d;
						bestCell = candidate;
						found = true;
					}
				}
			}

			if (found)
				return bestCell;
		}

		return nearest; // fallback
	}

	bool isCellOccupied(vec2 cell, Entity exclude)
	{
		const float threshold = GRID_CELL * 0.25f;
		for (const auto& b : m_balls)
		{
			if (b.entity == exclude)
				continue;
			if (!m_tempRegistry->hasComponent<Transform>(b.entity))
				continue;
			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			vec2 p(t.position.x, t.position.y);
			if (std::abs(p.x - cell.x) < threshold && std::abs(p.y - cell.y) < threshold)
				return true;
		}
		return false;
	}

	void handleCameraInput()
	{
		float wheel = 0.0f;
		if (m_tempSvc->input().getMouseButtonDown(Input::WheelUp))
			wheel += 1.0f;
		if (m_tempSvc->input().getMouseButtonDown(Input::WheelDown))
			wheel -= 1.0f;
		if (wheel != 0.0f)
			zoomCamera(wheel);

		const bool middleDown = m_tempSvc->input().getMouseButton(Input::MiddleClick);
		if (middleDown && m_middleWasDown)
		{
			panCamera(vec2(m_tempSvc->input().getMouseDeltaXRaw(), -m_tempSvc->input().getMouseDeltaYRaw()));
		}
		m_middleWasDown = middleDown;
	}

	void zoomCamera(float steps)
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());

		const float oldDistance = cam.position.z;
		const float newDistance = (std::clamp)(oldDistance * std::pow(1.0f - CAMERA_ZOOM_STEP, steps),
											   CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);
		if (std::abs(newDistance - oldDistance) < 0.0001f)
			return;

		const vec2 mouse(m_tempSvc->input().getMouseX(), m_tempSvc->input().getMouseY());
		const vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, mouse);
		const float halfWidth = (std::max)(1.0f, static_cast<float>(Display::width) * 0.5f);
		const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
		const float scale = newDistance / halfHeight;

		cam.position.z = newDistance;
		cam.position.x = world.x - (mouse.x - halfWidth) * scale;
		cam.position.y = world.y - (mouse.y - halfHeight) * scale;
		m_tempRegistry->setComponentDirty(cam);
		g_cameraPositon = cam.position;
	}

	void panCamera(vec2 screenDelta)
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
		const float scale = cam.position.z / halfHeight;

		cam.position.x -= screenDelta.x * scale;
		cam.position.y -= screenDelta.y * scale;
		m_tempRegistry->setComponentDirty(cam);
		g_cameraPositon = cam.position;
	}

	void handleRightMouseInput()
	{
		const bool rightDown = m_tempSvc->input().getMouseButton(Input::RightClick);

		if (rightDown && !m_rightWasDown)
		{
			startRightDrag();
		}
		else if (rightDown && m_rightWasDown)
		{
			updateRightDrag();
		}
		else if (!rightDown && m_rightWasDown)
		{
			endRightDrag();
		}

		m_rightWasDown = rightDown;
	}

	void startRightDrag()
	{
		const Entity hit = pickBallAtMouse();
		const int simulationId = (hit == static_cast<Entity>(-1)) ? -1 : getSimulationId(hit);

		if (hit == static_cast<Entity>(-1) || simulationId < 0)
		{
			// Nothing under the cursor: the right button always drags the view.
			m_dragMode = DragMode::Camera;
			return;
		}

		m_dragMode = DragMode::Ball;
		m_draggedBall = hit;
		m_draggedSimulationId = simulationId;
		m_keepFixedAfterDrag = false;

		auto& rb = m_tempRegistry->getComponent<RigidBody2D>(m_draggedBall);
		rb.isFixed = true;
		m_tempRegistry->setComponentDirty(rb);

		vec2 startPos = getMouseWorldPosition();
		if (gridEnabled())
			startPos = snapToGrid(startPos, m_draggedBall);

		auto& t = m_tempRegistry->getComponent<Transform>(m_draggedBall);
		t.position = vec3(startPos.x, startPos.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		if (gridEnabled())
		{
			resizeLinksForBall(m_draggedBall);
		}
	}

	void updateRightDrag()
	{
		if (m_dragMode == DragMode::Camera)
		{
			panCamera(vec2(m_tempSvc->input().getMouseDeltaXRaw(), -m_tempSvc->input().getMouseDeltaYRaw()));
			return;
		}

		if (m_dragMode != DragMode::Ball || m_draggedBall == static_cast<Entity>(-1) || m_draggedSimulationId < 0)
			return;

		if (m_tempSvc->input().getKeyDown(Input::F))
		{
			m_keepFixedAfterDrag = true;
		}

		vec2 dragPos = getMouseWorldPosition();
		if (gridEnabled())
			dragPos = snapToGrid(dragPos, m_draggedBall);

		auto& t = m_tempRegistry->getComponent<Transform>(m_draggedBall);
		const vec2 previousPosition(t.position.x, t.position.y);
		t.position = vec3(dragPos.x, dragPos.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		if (gridEnabled() && glm::length2(dragPos - previousPosition) > 0.0f)
		{
			// Snapping to a new cell changes the geometry: let the links follow.
			resizeLinksForBall(m_draggedBall);
		}
	}

	void endRightDrag()
	{
		if (m_dragMode == DragMode::Ball && m_draggedBall != static_cast<Entity>(-1) && m_draggedSimulationId >= 0)
		{
			if (!m_keepFixedAfterDrag)
			{
				auto& rb = m_tempRegistry->getComponent<RigidBody2D>(m_draggedBall);
				rb.isFixed = false;
				m_tempRegistry->setComponentDirty(rb);
			}

			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = -1;
			m_keepFixedAfterDrag = false;
		}

		m_dragMode = DragMode::None;
	}

	void handleLeftClickInput()
	{
		if (!m_tempSvc->input().getMouseButtonDown(Input::LeftClick) || m_tempSvc->input().isUIClick())
		{
			return;
		}

		switch (m_toolMode)
		{
			case ToolMode::Add:
				spawnBallAtMouse();
				break;
			case ToolMode::Spring:
			case ToolMode::Distance:
			case ToolMode::Remove:
				handleConstraintClick();
				break;
			case ToolMode::Modify:
				// Link distance dragging is handled by handleLinkInteraction().
				break;
			case ToolMode::TagEditor:
				selectTagEntity();
				break;
			case ToolMode::Material:
				paintBallAtMouse();
				break;
		}
	}

	void handleConstraintClick()
	{
		const Entity hit = pickBallAtMouse();

		if (m_constraintStartBall == static_cast<Entity>(-1))
		{
			if (hit != static_cast<Entity>(-1))
			{
				m_constraintStartBall = hit;
			}
			return;
		}

		if (hit != static_cast<Entity>(-1) && hit != m_constraintStartBall)
		{
			if (m_toolMode == ToolMode::Remove)
			{
				removeConstraintLink(m_constraintStartBall, hit);
			}
			else
			{
				const LinkType type = (m_toolMode == ToolMode::Distance) ? LinkType::Distance : LinkType::Spring;
				addConstraintLink(m_constraintStartBall, hit, type);
			}
		}

		m_constraintStartBall = static_cast<Entity>(-1);
	}

	Entity pickBallAtMouse()
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		vec2 world = ECS::Camera::screenPositionToWorldPosition2D(
			cam, vec2(m_tempSvc->input().getMouseX(), m_tempSvc->input().getMouseY()));

		float best = BALL_HIT_RADIUS;
		Entity bestEntity = static_cast<Entity>(-1);

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
				continue;

			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			vec2 p(t.position.x, t.position.y);
			float d = length(world - p);
			if (d < best)
			{
				best = d;
				bestEntity = b.entity;
			}
		}

		return bestEntity;
	}

	bool linkExists(Entity a, Entity b)
	{
		for (const auto& link : m_links)
		{
			bool sameDir = (link.a == a && link.b == b);
			bool reverseDir = (link.a == b && link.b == a);
			if (sameDir || reverseDir)
				return true;
		}
		return false;
	}

	void addConstraintLink(Entity a, Entity b, LinkType type)
	{
		if (linkExists(a, b))
			return;

		if (!hasTransform(a) || !hasTransform(b))
			return;

		int idA = getSimulationId(a);
		int idB = getSimulationId(b);
		if (idA < 0 || idB < 0)
			return;

		auto& ta = m_tempRegistry->getComponent<Transform>(a);
		auto& tb = m_tempRegistry->getComponent<Transform>(b);
		vec2 pa(ta.position.x, ta.position.y);
		vec2 pb(tb.position.x, tb.position.y);
		float restDistance = length(pb - pa);
		restDistance = (std::max)(restDistance, 1.0f);

		Entity constraintEnt = m_tempRegistry->createEntity();
		if (type == LinkType::Distance)
		{
			auto& constraint = m_tempRegistry->addComponent<DistanceConstraint>(constraintEnt);
			constraint.entityA = a;
			constraint.entityB = b;
			constraint.distance = restDistance;
		}
		else
		{
			auto& spring = m_tempRegistry->addComponent<Spring>(constraintEnt);
			spring.entityA = a;
			spring.entityB = b;
			spring.stiffness = SPRING_STIFFNESS;
			spring.restDistance = restDistance;
		}

		uint16_t lineColor = (type == LinkType::Distance) ? 10 : 8;

		float lineVars[8]{};
		computeScreenLineParams(pa, pb, lineVars);
		Entity line = m_tempSvc->shapes().addUIShape(
			{.shapeId = DefaultShapes::LINE, .variables = lineVars, .material = lineColor});

		m_tempSvc->serialization().blacklistEntity(line);

		m_links.push_back({a, b, idA, idB, restDistance, line, type, constraintEnt});
	}

	void removeConstraintLink(Entity a, Entity b)
	{
		for (size_t i = 0; i < m_links.size();)
		{
			DistanceLink& link = m_links[i];
			bool sameDir = (link.a == a && link.b == b);
			bool reverseDir = (link.a == b && link.b == a);
			if (!sameDir && !reverseDir)
			{
				++i;
				continue;
			}

			if (m_draggedLinkConstraint == link.constraintEntity)
			{
				m_draggedLinkConstraint = static_cast<Entity>(-1);
			}

			m_tempRegistry->destroyEntity(link.constraintEntity);
			m_tempRegistry->destroyEntity(link.lineEntity);
			m_links.erase(m_links.begin() + i);
		}
	}

	void handleLinkInteraction()
	{
		// A fresh press on a link starts a distance drag in Modify mode, and
		// removes the link in Remove mode (ball clicks take priority there).
		const bool freshClick =
			m_tempSvc->input().getMouseButtonDown(Input::LeftClick) && !m_tempSvc->input().isUIClick();

		if (freshClick)
		{
			if (m_toolMode == ToolMode::Modify)
			{
				const int index = pickLinkAtMouse();
				if (index >= 0)
				{
					m_draggedLinkConstraint = m_links[static_cast<size_t>(index)].constraintEntity;
					m_linkDragStartX = m_tempSvc->input().getMouseX();
					m_linkDragStartDist = m_links[static_cast<size_t>(index)].restDistance;
				}
			}
			else if (m_toolMode == ToolMode::Remove && pickBallAtMouse() == static_cast<Entity>(-1))
			{
				const int index = pickLinkAtMouse();
				if (index >= 0)
				{
					removeLinkAt(static_cast<size_t>(index));
					return;
				}
			}
		}

		if (!m_tempSvc->input().getMouseButton(Input::LeftClick))
		{
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			return;
		}

		DistanceLink* dragged = findLinkByConstraint(m_draggedLinkConstraint);
		if (dragged == nullptr)
		{
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			return;
		}

		const float dx = (m_tempSvc->input().getMouseX() - m_linkDragStartX) * 0.3f;
		float newDist = std::round((m_linkDragStartDist + dx) * 10.0f) / 10.0f;
		newDist = (std::clamp)(newDist, 1.0f, 10.0f);
		dragged->restDistance = newDist;

		// Route the change through the ECS component: PhysicsSystem2D forwards
		// it to the simulation from the main thread. Calling into the
		// simulation from onPhysicsStep would self-deadlock on its structural
		// mutex.
		applyLinkDistance(*dragged);
	}

	DistanceLink* findLinkByConstraint(Entity constraintEntity)
	{
		if (constraintEntity == static_cast<Entity>(-1))
		{
			return nullptr;
		}

		for (DistanceLink& link : m_links)
		{
			if (link.constraintEntity == constraintEntity)
			{
				return &link;
			}
		}
		return nullptr;
	}

	int pickLinkAtMouse()
	{
		const vec2 mouse = getMouseWorldPosition();
		float best = linkPickRadiusWorld();
		int bestIndex = -1;

		for (size_t i = 0; i < m_links.size(); ++i)
		{
			const DistanceLink& link = m_links[i];
			if (!hasTransform(link.a) || !hasTransform(link.b))
			{
				continue;
			}

			const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
			const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
			const float d = distancePointToSegment(mouse, vec2(ta.position.x, ta.position.y),
												   vec2(tb.position.x, tb.position.y));
			if (d < best)
			{
				best = d;
				bestIndex = static_cast<int>(i);
			}
		}

		return bestIndex;
	}

	float linkPickRadiusWorld()
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
		return LINK_PICK_PIXELS * (cam.position.z / halfHeight);
	}

	static float distancePointToSegment(vec2 p, vec2 a, vec2 b)
	{
		const vec2 ab = b - a;
		const float lengthSq = glm::length2(ab);
		if (lengthSq <= 0.0001f)
		{
			return glm::length(p - a);
		}

		const float t = (std::clamp)(glm::dot(p - a, ab) / lengthSq, 0.0f, 1.0f);
		return glm::length(p - (a + ab * t));
	}

	void updateConstraintLines()
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float lineWidth = (m_toolMode == ToolMode::Modify) ? MODIFY_LINE_WIDTH : LINE_WIDTH;

		for (auto& link : m_links)
		{
			if (!hasTransform(link.a) || !hasTransform(link.b) || !hasUIShape(link.lineEntity))
				continue;

			const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
			const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);

			vec2 aWorld(ta.position.x, ta.position.y);
			vec2 bWorld(tb.position.x, tb.position.y);
			vec2 aScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, aWorld);
			vec2 bScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, bWorld);

			auto& ui = m_tempRegistry->getComponent<UIShape>(link.lineEntity);
			ui.parameters[0] = aScreen.x;
			ui.parameters[1] = aScreen.y;
			ui.parameters[2] = bScreen.x;
			ui.parameters[3] = bScreen.y;
			ui.parameters[4] = lineWidth;
		}
	}

	void computeScreenLineParams(const vec2& aWorld, const vec2& bWorld, float outParams[8])
	{
		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		vec2 aScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, aWorld);
		vec2 bScreen = ECS::Camera::worldPosition2DToScreenPosition(cam, bWorld);

		outParams[0] = aScreen.x;
		outParams[1] = aScreen.y;
		outParams[2] = bScreen.x;
		outParams[3] = bScreen.y;
		outParams[4] = LINE_WIDTH;
	}

	int getSimulationId(Entity e)
	{
		for (const auto& b : m_balls)
		{
			if (b.entity == e)
				return b.simulationId;
		}
		return -1;
	}

	// -----------------------------------------------------------------------
	// Tag editor UI
	// -----------------------------------------------------------------------

	void updateTagEditor()
	{
		// Hide ring if no entity selected
		if (m_tagSelectedEntity == static_cast<Entity>(-1))
		{
			if (m_tagCircleOuter != static_cast<Entity>(-1))
			{
				m_tempRegistry->getComponent<UIShape>(m_tagCircleOuter).parameters[2] = 0.0f;
				m_tempRegistry->getComponent<UIShape>(m_tagCircleInner).parameters[2] = 0.0f;
			}
			return;
		}

		if (!hasTransform(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
			return;
		}

		// Lazy-initialize the ring indicator shapes on first use
		if (m_tagCircleOuter == static_cast<Entity>(-1))
		{
			float p[8]{};
			m_tagCircleOuter = m_tempSvc->shapes().addUIShape({.shapeId = DefaultShapes::CIRCLE,
															   .variables = p,
															   .material = 7,
															   .combination = CombinationType::Addition,
															   .group = TAG_RING_GROUP});
			m_tempSvc->serialization().blacklistEntity(m_tagCircleOuter);

			m_tagCircleInner = m_tempSvc->shapes().addUIShape({.shapeId = DefaultShapes::CIRCLE,
															   .variables = p,
															   .material = 7,
															   .combination = CombinationType::Subtraction,
															   .group = TAG_RING_GROUP});
			m_tempSvc->serialization().blacklistEntity(m_tagCircleInner);
		}

		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const auto& ht = m_tempRegistry->getComponent<Transform>(m_tagSelectedEntity);
		vec2 world(ht.position.x, ht.position.y);
		vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, world);

		auto& outer = m_tempRegistry->getComponent<UIShape>(m_tagCircleOuter);
		outer.parameters[0] = screen.x;
		outer.parameters[1] = screen.y;
		outer.parameters[2] = TAG_OUTER_RADIUS;

		auto& inner = m_tempRegistry->getComponent<UIShape>(m_tagCircleInner);
		inner.parameters[0] = screen.x;
		inner.parameters[1] = screen.y;
		inner.parameters[2] = TAG_INNER_RADIUS;
	}

	void selectTagEntity()
	{
		m_tagSelectedEntity = pickBallAtMouse();
	}

	void paintBallAtMouse()
	{
		Entity hit = pickBallAtMouse();
		if (hit == static_cast<Entity>(-1))
			return;

		auto& dot = m_tempRegistry->getComponent<Dot>(hit);
		dot.materialId = static_cast<unsigned int>(m_selectedMaterial);
	}

	bool hasTransform(Entity e)
	{
		auto arr = m_tempRegistry->getComponentArray<Transform>();
		return arr->hasData(e);
	}

	bool hasUIShape(Entity e)
	{
		auto arr = m_tempRegistry->getComponentArray<UIShape>();
		return arr->hasData(e);
	}

	void loadMolecule(const std::string& path)
	{
		// Remember constraint count before loading so we can find new ones
		size_t prevSpringCount = m_tempRegistry->getComponentArray<Spring>()->getSize();
		size_t prevDistCount = m_tempRegistry->getComponentArray<DistanceConstraint>()->getSize();

		// Load the file — creates new entities / rigid bodies / constraints
		TagMap loadedTags = m_tempSvc->serialization().loadWeirdFile(path);

		// Apply loaded tags to the scene
		for (const auto& [name, entity] : loadedTags)
		{
			m_tempSvc->tags().tag(entity, name);
		}

		// Collect new balls: find entities with both Dot and RigidBody2D
		// that are not already tracked
		auto dotArray = m_tempRegistry->getComponentArray<Dot>();
		auto rbArray = m_tempRegistry->getComponentArray<RigidBody2D>();

		// Build a set of already-tracked entities for fast lookup
		std::unordered_set<Entity> existingBalls;
		for (const auto& b : m_balls)
			existingBalls.insert(b.entity);

		// Map from simulationId → entity for newly loaded balls
		std::unordered_map<int, Entity> simIdToEntity;

		for (size_t i = 0; i < rbArray->getSize(); i++)
		{
			Entity e = rbArray->getEntityAtIdx(i);
			if (existingBalls.count(e))
				continue;
			if (!dotArray->hasData(e))
				continue;

			auto& rb = rbArray->getDataAtIdx(i);
			int simId = static_cast<int>(rb.simulationId);
			m_balls.push_back({e, simId});
			simIdToEntity[simId] = e;
		}

		// Collect new constraints and create visual links
		auto springArray = m_tempRegistry->getComponentArray<Spring>();
		size_t newSprings = 0;
		for (size_t i = prevSpringCount; i < springArray->getSize(); i++)
		{
			Entity springEnt = springArray->getEntityAtIdx(i);
			auto& spring = springArray->getDataAtIdx(i);

			Entity a = spring.entityA;
			Entity b = spring.entityB;

			if (!hasTransform(a) || !hasTransform(b))
				continue;
			if (!m_tempRegistry->hasComponent<RigidBody2D>(a) || !m_tempRegistry->hasComponent<RigidBody2D>(b))
				continue;

			uint16_t lineColor = 8;
			vec2 pa(m_tempRegistry->getComponent<Transform>(a).position);
			vec2 pb(m_tempRegistry->getComponent<Transform>(b).position);

			float lineVars[8]{};
			computeScreenLineParams(pa, pb, lineVars);
			Entity line = m_tempSvc->shapes().addUIShape(
				{.shapeId = DefaultShapes::LINE, .variables = lineVars, .material = lineColor});

			m_tempSvc->serialization().blacklistEntity(line);

			int idA = m_tempRegistry->getComponent<RigidBody2D>(a).simulationId;
			int idB = m_tempRegistry->getComponent<RigidBody2D>(b).simulationId;
			m_links.push_back({a, b, idA, idB, spring.restDistance, line, LinkType::Spring, springEnt});
			newSprings++;
		}

		auto distArray = m_tempRegistry->getComponentArray<DistanceConstraint>();
		size_t newDists = 0;
		for (size_t i = prevDistCount; i < distArray->getSize(); i++)
		{
			Entity distEnt = distArray->getEntityAtIdx(i);
			auto& dist = distArray->getDataAtIdx(i);

			Entity a = dist.entityA;
			Entity b = dist.entityB;

			if (!hasTransform(a) || !hasTransform(b))
				continue;
			if (!m_tempRegistry->hasComponent<RigidBody2D>(a) || !m_tempRegistry->hasComponent<RigidBody2D>(b))
				continue;

			uint16_t lineColor = 10;
			vec2 pa(m_tempRegistry->getComponent<Transform>(a).position);
			vec2 pb(m_tempRegistry->getComponent<Transform>(b).position);

			float lineVars[8]{};
			computeScreenLineParams(pa, pb, lineVars);
			Entity line = m_tempSvc->shapes().addUIShape(
				{.shapeId = DefaultShapes::LINE, .variables = lineVars, .material = lineColor});

			m_tempSvc->serialization().blacklistEntity(line);

			int idA = m_tempRegistry->getComponent<RigidBody2D>(a).simulationId;
			int idB = m_tempRegistry->getComponent<RigidBody2D>(b).simulationId;
			m_links.push_back({a, b, idA, idB, dist.distance, line, LinkType::Distance, distEnt});
			newDists++;
		}

		std::string loadMsg = "[MoleculeEditor] Loaded " + std::to_string(simIdToEntity.size()) + " balls and " +
							  std::to_string(newSprings + newDists) + " links from " + path;
		WeirdEngine::Logger::log(loadMsg);
	}

	void onEntityShapeCollision(Registry& registry, ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		m_tempRegistry = &registry;
		m_tempSvc = &services;
	}
};
