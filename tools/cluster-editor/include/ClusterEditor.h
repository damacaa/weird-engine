#pragma once

#include <weird-engine.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
#include "weird-physics/components/Spring.h"
#include "weird-physics/SimulationID.h"
#include <glm/gtx/norm.hpp>

extern WeirdEngine::vec3 g_cameraPositon;
inline std::string g_initialClusterPath;

using namespace WeirdEngine;

class ClusterEditor : public Scene2D
{
public:
	ClusterEditor() {}

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

	enum class RemoveTarget
	{
		Auto,
		Balls,
		Links
	};

	enum class LinkFilterMode
	{
		All,
		Springs,
		Distance
	};

	struct BallInfo
	{
		Entity entity = INVALID_ENTITY;
		SimulationID simulationId = INVALID_SIMULATION_ID;
	};

	struct DistanceLink
	{
		Entity a = INVALID_ENTITY;
		Entity b = INVALID_ENTITY;
		SimulationID simulationIdA = INVALID_SIMULATION_ID;
		SimulationID simulationIdB = INVALID_SIMULATION_ID;
		float restDistance = 0.0f;
		LinkType type = LinkType::Spring;
		Entity constraintEntity = INVALID_ENTITY;
	};

	std::vector<BallInfo> m_balls;
	std::vector<DistanceLink> m_links;
	std::vector<Entity> m_selectedLinkConstraints;

	Entity m_draggedBall = static_cast<Entity>(-1);
	SimulationID m_draggedSimulationId = INVALID_SIMULATION_ID;
	Entity m_constraintStartBall = static_cast<Entity>(-1);
	Entity m_draggedLinkConstraint = static_cast<Entity>(-1);
	Entity m_uiHoveredLink = static_cast<Entity>(-1);
	float m_linkDragStartX = 0.0f;
	float m_linkDragStartDist = 0.0f;
	bool m_modifyPressPending = false;
	bool m_modifyDragActive = false;
	Entity m_modifyPressedLink = static_cast<Entity>(-1);
	float m_modifyPressMouseX = 0.0f;
	float m_modifyPressMouseY = 0.0f;
	bool m_keepFixedAfterDrag = false;
	bool m_rightWasDown = false;
	bool m_middleWasDown = false;
	bool m_gravityEnabled = true;
	bool m_isSimulating = false;
	float m_playGravity = -10.0f;
	float m_playDamping = 0.01f;

	struct ClusterSnapshotState
	{
		std::vector<BallInfo> balls;
		std::vector<DistanceLink> links;
		Entity tagSelectedEntity = static_cast<Entity>(-1);
		std::unordered_map<Entity, std::string> names;
		std::unordered_map<Entity, std::vector<std::string>> tags;
	};

	struct UndoBallState
	{
		vec2 position = vec2(0.0f);
		unsigned int materialId = 1;
		bool isFixed = false;
		std::string name;
		std::vector<std::string> tags;
	};

	struct UndoLinkState
	{
		size_t indexA = 0;
		size_t indexB = 0;
		LinkType type = LinkType::Spring;
		float restDistance = 1.0f;
		float stiffness = SPRING_STIFFNESS;
	};

	struct ClusterUndoState
	{
		std::vector<UndoBallState> balls;
		std::vector<UndoLinkState> links;
		int selectedBallIndex = -1;
		std::vector<size_t> selectedLinkIndices;
	};

	static constexpr size_t MAX_UNDO_HISTORY = 64;
	std::vector<ClusterUndoState> m_undoStack;
	std::vector<ClusterUndoState> m_redoStack;
	ClusterUndoState m_pendingBallDragUndo;
	bool m_hasPendingBallDragUndo = false;
	vec2 m_dragStartBallPos = vec2(0.0f);
	bool m_wasFixedBeforeDrag = false;
	bool m_resizedLinksDuringBallDrag = false;
	ClusterUndoState m_pendingLinkDragUndo;
	bool m_hasPendingLinkDragUndo = false;
	ClusterUndoState m_uiDragUndoState;
	bool m_hasUiDragUndo = false;

	ClusterSnapshotState m_playSnapshot;
	bool m_hasPlaySnapshot = false;
	ClusterSnapshotState m_manualSnapshot;
	bool m_hasManualSnapshot = false;
	GridMode m_grid = GridMode::Off;
	DragMode m_dragMode = DragMode::None;
	RemoveTarget m_removeTarget = RemoveTarget::Auto;
	int m_selectedMaterial = 1;

	ToolMode m_toolMode = ToolMode::Add;

	// ImGui editor UI state
	char m_fileNameBuf[128] = "cluster";
	char m_tagBuf[128] = {};
	char m_newTagBuf[128] = {};
	char m_linkDistInputBuf[32] = {};
	char m_linkStiffInputBuf[32] = {};
	Entity m_syncedLinkPrimary = static_cast<Entity>(-1);
	size_t m_syncedLinkSelCount = 0;
	float m_syncedLinkDist = -1.0f;
	float m_syncedLinkK = -1.0f;
	LinkFilterMode m_linkFilter = LinkFilterMode::All;
	Entity m_tagBufferEntity = static_cast<Entity>(-1);
	float m_menuBarHeight = 0.0f;
	bool m_cameraCentered = false;
	bool m_showControls = false;

	// Scene file state
	std::string m_currentFilePath;
	std::mutex m_fileMutex;
	std::string m_pendingLoadPath;
	std::string m_pendingSavePath;
	bool m_pendingNewCluster = false;

	// Tag editor state
	Entity m_tagSelectedEntity = static_cast<Entity>(-1);
	Entity m_tagCircleOuter = static_cast<Entity>(-1);
	Entity m_tagCircleInner = static_cast<Entity>(-1);

	// Relaxation state
	bool m_isRelaxing = false;
	bool m_relaxationStable = false;
	int m_relaxationStableFrames = 0;
	int m_relaxationStepCount = 0;
	float m_relaxationMaxDrift = 0.0f;
	float m_repulsionStrength = 6.0f;
	float m_relaxationSpeed = 1.0f;
	std::unordered_map<Entity, vec2> m_relaxationVelocities;

	// Cluster rotation UI state
	bool m_isRotatingDrag = false;
	bool m_hasRotateUndo = false;
	ClusterUndoState m_rotateUndoState;
	float m_rotateDragAngleDeg = 0.0f;
	vec2 m_rotateStartCenter = vec2(0.0f);
	std::vector<vec2> m_rotateStartPositions;

	static constexpr float BALL_WORLD_RADIUS = 0.5f;
	static constexpr float BALL_DIAMETER = BALL_WORLD_RADIUS * 2.0f;
	static constexpr float BALL_HIT_RADIUS = 0.56f;
	static constexpr float BALL_CORE_RADIUS = 0.28f;
	static constexpr float BALL_PICK_MIN_PIXELS = 8.0f;
	static constexpr float LINK_PICK_PIXELS = 10.0f;
	static constexpr float LINK_PICK_MAX_WORLD = 0.45f;
	static constexpr float MODIFY_DRAG_THRESHOLD_PX = 4.0f;
	static constexpr float MIN_LINK_DISTANCE = 0.25f;
	static constexpr float MAX_LINK_DISTANCE = 25.0f;

	static constexpr float minAllowedLinkDistance(LinkType type)
	{
		return (type == LinkType::Distance) ? BALL_DIAMETER : MIN_LINK_DISTANCE;
	}
	static constexpr float STIFFNESS_MIN = 0.001f;
	static constexpr float STIFFNESS_MAX = 1.0f;
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

	static bool isValidEntityId(Entity e)
	{
		return e != static_cast<Entity>(-1) && e != INVALID_ENTITY;
	}

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		m_tempRegistry = &registry;
		m_tempSvc = &services;

		g_cameraPositon.x = 0.0f;
		g_cameraPositon.y = 0.0f;
		m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity()).position = g_cameraPositon;

		services.render().getBackground().type = BackgroundType::Grid;
		services.render().getBackground().scale = 1.0f;
		services.render().getBackground().parallax = 1.0f;

		// Request neutral simulation behavior for this editor scene.
		services.physics().setGravity(0.0f);
		services.physics().setDamping(1.0f);

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = m_tempSvc->materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		{
			Entity outside = m_tempSvc->shapes().addShape({.shapeId = DefaultShapes::Circle,
														   .variables = {0.0f, 0.0f, 3000.0f},
														   .material = 0,
														   .combination = CombinationType::Addition});

			Entity inside = m_tempSvc->shapes().addShape({.shapeId = DefaultShapes::Box,
														  .variables = {0.0f, 0.0f, 20.0f, 20.0f},
														  .material = 0,
														  .combination = CombinationType::Subtraction});

			m_tempSvc->serialization().blacklistEntity(outside);
			m_tempSvc->serialization().blacklistEntity(inside);
		}
		if (!g_initialClusterPath.empty() && std::filesystem::exists(g_initialClusterPath))
		{
			loadCluster(g_initialClusterPath);
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
			if (m_isSimulating)
			{
				setSimulation(false);
			}
			m_tempSvc->sceneControl().goToNextScene();
			return;
		}

		const bool wantText = ImGui::GetIO().WantTextInput;
		const bool ctrlHeld = m_tempSvc->input().getKey(Input::LeftCtrl);

		if (!wantText && m_tempSvc->input().getKeyDown(Input::Space))
		{
			toggleSimulation();
		}

		if (!wantText && !ctrlHeld)
		{
			const Input::KeyCode toolKeys[7] = {Input::Num1, Input::Num2, Input::Num3, Input::Num4,
												Input::Num5, Input::Num6, Input::Num7};
			const ToolMode keyModes[7] = {ToolMode::Add,	ToolMode::Spring,	 ToolMode::Distance, ToolMode::Modify,
										  ToolMode::Remove, ToolMode::TagEditor, ToolMode::Material};
			for (int i = 0; i < 7; ++i)
			{
				if (m_tempSvc->input().getKeyDown(toolKeys[i]))
				{
					setToolMode(keyModes[i]);
				}
			}

			if (m_tempSvc->input().getKeyDown(Input::Esc))
			{
				if (isValidEntityId(m_constraintStartBall))
				{
					m_constraintStartBall = static_cast<Entity>(-1);
				}
				else if (isValidEntityId(m_tagSelectedEntity))
				{
					clearTagSelection();
				}
				else if (!m_selectedLinkConstraints.empty())
				{
					clearLinkSelection();
				}
			}

			if (m_tempSvc->input().getKeyDown(Input::F) && m_dragMode != DragMode::Ball)
			{
				toggleFixHoveredOrSelectedBall();
			}
		}

		if (!wantText &&
			(m_tempSvc->input().getKeyDown(Input::Delete) || m_tempSvc->input().getKeyDown(Input::Backspace)))
		{
			handleDeleteKey();
		}

		if (ctrlHeld && !wantText)
		{
			const bool shiftHeld = m_tempSvc->input().getKey(Input::LeftShift);
			if (m_tempSvc->input().getKeyDown(Input::Z))
			{
				if (shiftHeld)
				{
					redo();
				}
				else
				{
					undo();
				}
			}
			else if (m_tempSvc->input().getKeyDown(Input::Y))
			{
				redo();
			}
		}

		if (ctrlHeld && m_tempSvc->input().getKeyDown(Input::S))
		{
			saveCluster();
		}

		if (ctrlHeld && m_tempSvc->input().getKeyDown(Input::O))
		{
			openLoadFileDialog();
		}

		if (ctrlHeld && m_tempSvc->input().getKeyDown(Input::L))
		{
			openLoadFileDialog();
		}

		if (ctrlHeld && m_tempSvc->input().getKeyDown(Input::N))
		{
			m_pendingNewCluster = true;
		}

		pollFileActions();

		if (m_pendingNewCluster)
		{
			m_pendingNewCluster = false;
			if (!m_balls.empty())
			{
				pushUndoState();
			}
			clearCluster();
		}

		handleCameraInput();
		handleRightMouseInput();
		handleLeftClickInput();
		handleLeftReleaseInput();
		handleLinkInteraction();
		if (m_isRelaxing)
		{
			stepRelaxation(m_tempSvc->time().deltaTime());
		}
		pruneLinkSelection();
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
			if (!m_balls.empty())
			{
				frameCluster();
			}
			else
			{
				centerCameraOnSceneView();
			}
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
				if (ImGui::MenuItem("New Cluster", "Ctrl+N"))
				{
					m_pendingNewCluster = true;
				}
				if (ImGui::MenuItem("Open Cluster...", "Ctrl+O"))
				{
					openLoadFileDialog();
				}

				const bool hasFile = !m_currentFilePath.empty();
				if (ImGui::MenuItem(hasFile ? "Save Cluster" : "Save Cluster...", "Ctrl+S"))
				{
					saveCluster();
				}
				if (ImGui::MenuItem("Save Cluster As..."))
				{
					openSaveFileDialog();
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Exit Tool", "Q"))
				{
					if (m_isSimulating)
					{
						setSimulation(false);
					}
					m_tempSvc->sceneControl().goToNextScene();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo()))
				{
					undo();
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, canRedo()))
				{
					redo();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Frame Cluster"))
				{
					frameCluster();
				}
				if (ImGui::MenuItem("Reset Camera"))
				{
					auto& camTransform = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
					camTransform.position.z = 35.0f;
					m_tempRegistry->setComponentDirty(camTransform);
					centerCameraOnSceneView();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Simulation"))
			{
				if (ImGui::MenuItem(m_isSimulating ? "Stop Simulation" : "Play Simulation", "Space"))
				{
					toggleSimulation();
				}
				ImGui::Separator();
				if (ImGui::DragFloat("Play Gravity", &m_playGravity, 0.1f, -50.0f, 50.0f, "%.1f") && m_isSimulating)
				{
					m_tempSvc->physics().setGravity(m_gravityEnabled ? m_playGravity : 0.0f);
				}
				if (ImGui::DragFloat("Play Damping", &m_playDamping, 0.001f, 0.0f, 1.0f, "%.3f") && m_isSimulating)
				{
					m_tempSvc->physics().setDamping(m_playDamping);
				}
				if (ImGui::MenuItem("Take Snapshot"))
				{
					takeManualSnapshot();
				}
				if (ImGui::MenuItem("Restore Snapshot", nullptr, false, m_hasManualSnapshot))
				{
					restoreManualSnapshot();
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

		const char* labels[] = {"Add [1]",	  "Spring [2]", "Distance [3]", "Modify [4]",
								"Remove [5]", "Tag [6]",	"Material [7]"};
		const ToolMode modes[] = {ToolMode::Add,	ToolMode::Spring,	 ToolMode::Distance, ToolMode::Modify,
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
				setToolMode(modes[i]);
			}
			if (selected)
			{
				ImGui::PopStyleColor();
			}
			ImGui::PopID();
		}

		if (m_toolMode == ToolMode::Remove)
		{
			ImGui::Spacing();
			ImGui::TextDisabled("Remove Target:");
			const char* targetLabels[] = {"Auto", "Balls", "Links"};
			const RemoveTarget targetValues[] = {RemoveTarget::Auto, RemoveTarget::Balls, RemoveTarget::Links};
			const float thirdWidth = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
			for (int i = 0; i < 3; ++i)
			{
				if (i > 0)
				{
					ImGui::SameLine();
				}
				const bool active = (m_removeTarget == targetValues[i]);
				if (active)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
				}
				if (ImGui::Button(targetLabels[i], ImVec2(thirdWidth, 22.0f)))
				{
					m_removeTarget = targetValues[i];
				}
				if (active)
				{
					ImGui::PopStyleColor();
				}
			}
		}

		switch (m_toolMode)
		{
			case ToolMode::Add:
				ImGui::TextDisabled("Click to spawn a ball (F toggles pin).");
				break;
			case ToolMode::Spring:
			case ToolMode::Distance:
				ImGui::TextDisabled("Click or drag ball A -> B. Esc/RMB cancels.");
				break;
			case ToolMode::Modify:
				ImGui::TextDisabled("Click link to select (Shift = multi); drag sideways for distance.");
				break;
			case ToolMode::Remove:
				if (m_removeTarget == RemoveTarget::Auto)
				{
					ImGui::TextDisabled("Click ball core or link span (Shift = links only).");
				}
				else if (m_removeTarget == RemoveTarget::Balls)
				{
					ImGui::TextDisabled("Click a ball to delete it and attached links.");
				}
				else
				{
					ImGui::TextDisabled("Click a spring or distance link to delete it.");
				}
				break;
			case ToolMode::TagEditor:
				ImGui::TextDisabled("Left click a ball to inspect/edit name & tags.");
				break;
			case ToolMode::Material:
				ImGui::TextDisabled("Click a ball to paint with the selected material.");
				break;
		}
	}

	void renderOptionsSection()
	{
		if (!ImGui::CollapsingHeader("Options", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float buttonWidth = ImGui::GetContentRegionAvail().x;
		if (m_isSimulating)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.2f, 1.0f));
			if (ImGui::Button("Pause (Space)", ImVec2(buttonWidth, 26.0f)))
			{
				toggleSimulation();
			}
			ImGui::PopStyleColor();
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
			if (ImGui::Button("Play (Space)", ImVec2(buttonWidth, 26.0f)))
			{
				toggleSimulation();
			}
			ImGui::PopStyleColor();
		}

		if (ImGui::Checkbox("Gravity", &m_gravityEnabled) && m_isSimulating && m_tempSvc != nullptr)
		{
			m_tempSvc->physics().setGravity(m_gravityEnabled ? m_playGravity : 0.0f);
		}

		const char* gridItems[] = {"Off", "Square", "Hex"};
		int gridIndex = static_cast<int>(m_grid);
		if (ImGui::Combo("Grid Snap", &gridIndex, gridItems, IM_ARRAYSIZE(gridItems)))
		{
			m_grid = static_cast<GridMode>(gridIndex);
		}
	}

	void renderClustersSection()
	{
		if (!ImGui::CollapsingHeader("Clusters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float buttonWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;

		if (m_isRelaxing)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.25f, 0.25f, 1.0f));
			if (ImGui::Button("Stop Relaxing", ImVec2(buttonWidth, 26.0f)))
			{
				m_isRelaxing = false;
				m_relaxationVelocities.clear();
			}
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button("Center & Relax", ImVec2(buttonWidth, 26.0f)))
			{
				startRelaxation();
			}
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
			fitClustersToGrid();
		}

		const ConflictStats stats =
			(!m_links.empty() || m_balls.size() >= 2) ? computeConstraintConflictStats() : ConflictStats{};
		const bool disableRelax = (m_links.empty() && stats.overlappingPairs == 0);
		if (disableRelax)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Relax Distances", ImVec2(buttonWidth, 26.0f)))
		{
			relaxConstraintDistances();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Separate overlapping bodies (< %.1f), settle constraints to a feasible equilibrium "
							  "(>= %.1f), and update all rest distances.",
							  BALL_DIAMETER, BALL_DIAMETER);
		}
		if (disableRelax)
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		const bool noLinks = m_links.empty();
		if (noLinks)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Match Cur Dist", ImVec2(buttonWidth, 26.0f)))
		{
			applyAllCurrentDistances(true);
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Set every constraint's rest distance to its current endpoint distance without moving "
							  "balls.");
		}
		if (noLinks)
		{
			ImGui::EndDisabled();
		}

		const bool canMinimize = (m_balls.size() >= 2);
		if (!canMinimize)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Minimize Dist", ImVec2(buttonWidth, 24.0f)))
		{
			minimizeConstraintDistances();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Pull free balls inward cleanly without overlapping (< %.1f) and update all "
							  "constraint distances.",
							  BALL_DIAMETER);
		}
		if (!canMinimize)
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		if (ImGui::Button("Center Only", ImVec2(buttonWidth, 24.0f)))
		{
			centerClusters(true);
		}

		ImGui::Spacing();
		ImGui::Text("Rotate Around Center");
		const bool canRotate = (m_balls.size() >= 2);
		if (!canRotate)
		{
			ImGui::BeginDisabled();
		}
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::SliderFloat("##rotateDeg", &m_rotateDragAngleDeg, -180.0f, 180.0f, "%.1f deg"))
		{
			if (!m_isRotatingDrag)
			{
				if (collectBallPositionsAndCentroid(m_rotateStartPositions, m_rotateStartCenter))
				{
					m_rotateUndoState = captureUndoState();
					m_hasRotateUndo = true;
					m_isRotatingDrag = true;
					m_isRelaxing = false;
					m_relaxationVelocities.clear();
					invalidatePlaySnapshotIfSimulating();
				}
			}
			if (m_isRotatingDrag)
			{
				applyRotationFromPositions(m_rotateStartPositions, m_rotateStartCenter, m_rotateDragAngleDeg);
			}
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Drag to rotate all entities around their center (Ctrl+Click to type angle)");
		}
		if (m_isRotatingDrag && (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsItemActive()))
		{
			if (m_hasRotateUndo && std::abs(m_rotateDragAngleDeg) > 1e-4f)
			{
				commitUndoState(m_rotateUndoState);
			}
			resetRotateDragState();
		}

		const float rotBtnW = (ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f;
		const float stepAngles[4] = {-90.0f, -15.0f, 15.0f, 90.0f};
		const char* stepLabels[4] = {"-90", "-15", "+15", "+90"};
		const char* stepTips[4] = {"Rotate 90 deg CW", "Rotate 15 deg CW", "Rotate 15 deg CCW", "Rotate 90 deg CCW"};
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			if (ImGui::Button(stepLabels[i], ImVec2(rotBtnW, 20.0f)))
			{
				rotateClusterByDegrees(stepAngles[i]);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				ImGui::SetTooltip("%s", stepTips[i]);
			}
		}
		if (!canRotate)
		{
			ImGui::EndDisabled();
		}

		if (m_isRelaxing)
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Relaxing... (drift: %.4f, step %d)",
							   m_relaxationMaxDrift, m_relaxationStepCount);
		}
		else if (m_relaxationStable)
		{
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Status: Stable");
		}
		else
		{
			ImGui::TextDisabled("Status: Idle");
		}

		if (!m_links.empty() || m_balls.size() >= 2)
		{
			if (stats.overlappingPairs > 0 && stats.strainedLinks > 0)
			{
				ImGui::TextColored(ImVec4(0.96f, 0.42f, 0.32f, 1.0f), "Overlap: %zu pair(s) | Strain: %zu link(s)",
								   stats.overlappingPairs, stats.strainedLinks);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Overlapping rigidbodies (< %.1f) or strained constraints exist. Use Relax "
									  "Distances.",
									  BALL_DIAMETER);
				}
			}
			else if (stats.overlappingPairs > 0)
			{
				ImGui::TextColored(ImVec4(0.96f, 0.42f, 0.32f, 1.0f), "Overlap: %zu pair(s) (< %.1f)",
								   stats.overlappingPairs, BALL_DIAMETER);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Some rigidbodies overlap (< %.1f). Use Relax Distances to separate them.",
									  BALL_DIAMETER);
				}
			}
			else if (stats.strainedLinks > 0)
			{
				ImGui::TextColored(ImVec4(0.95f, 0.72f, 0.25f, 1.0f), "Strain: %zu link(s) (max err %.3f)",
								   stats.strainedLinks, stats.maxError);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip(
						"Some constraints do not match the current geometry or minimum length. Use Relax Distances or "
						"Match Cur Dist.");
				}
			}
			else if (!m_links.empty())
			{
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Constraints: All feasible");
			}
		}

		if (ImGui::TreeNode("Relaxation Settings"))
		{
			ImGui::SliderFloat("Repulsion", &m_repulsionStrength, 1.0f, 20.0f, "%.1f");
			ImGui::SliderFloat("Speed", &m_relaxationSpeed, 0.2f, 3.0f, "%.1f");
			ImGui::TreePop();
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

		if (ImGui::Begin("##ClusterPanel", nullptr, flags))
		{
			renderToolsSection();
			ImGui::Spacing();
			renderOptionsSection();
			ImGui::Spacing();
			renderClustersSection();
			ImGui::Spacing();
			renderMaterialsSection();
			ImGui::Spacing();
			renderTagSection();
			ImGui::Spacing();
			renderSelectedLinkSection();
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

		ImGui::SetNextWindowSize(ImVec2(430.0f, 390.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(40.0f, m_menuBarHeight + 40.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Cluster Editor - Controls", &m_showControls))
		{
			ImGui::TextWrapped("Left click applies the active tool (1-7 switches tools):");
			ImGui::BulletText("1 Add: click to spawn a ball with the selected material");
			ImGui::BulletText("2 Spring / 3 Distance: click A then B, or drag A -> B (Esc/RMB cancels)");
			ImGui::BulletText(
				"4 Modify: click a link to select & edit exactly; drag sideways for distance (Shift = fine)");
			ImGui::BulletText("5 Remove: delete balls or links (Auto / Balls / Links; Shift = links only)");
			ImGui::BulletText("6 Tag: left click a ball to inspect/edit name, tags, and pin state");
			ImGui::BulletText("7 Material: click a ball to repaint it");
			ImGui::Separator();
			ImGui::BulletText("Delete / Backspace: delete hovered target, selected link(s), or selected ball");
			ImGui::BulletText("F: pin / unpin hovered or selected ball (or hold during right-drag)");
			ImGui::BulletText("Esc: cancel pending link, or clear ball / link selection");
			ImGui::Separator();
			ImGui::TextWrapped("Right click always drags: hold over a ball to move it (preserving pin state), "
							   "or over empty space to pan the camera. Hold Ctrl during ball drag to update attached "
							   "constraint distances to match.");
			ImGui::TextWrapped("Middle mouse drag pans the camera; mouse wheel zooms.");
			ImGui::Separator();
			ImGui::TextWrapped("With grid snapping on (or while holding Ctrl during right-drag), moved atoms update "
							   "their links to the new distance.");
			ImGui::Separator();
			ImGui::BulletText("Ctrl+Z / Ctrl+Shift+Z (or Ctrl+Y): undo / redo");
			ImGui::BulletText("Ctrl+N / Ctrl+O / Ctrl+S: new / open / save .weirdcluster");
			ImGui::BulletText("Space: play / pause physics simulation (Q: exit tool)");
		}
		ImGui::End();
	}

	static float stiffnessToNormalizedLog(float k)
	{
		const float clamped = (std::clamp)(k, STIFFNESS_MIN, STIFFNESS_MAX);
		const float minLog = std::log10(STIFFNESS_MIN);
		const float maxLog = std::log10(STIFFNESS_MAX);
		return (std::clamp)((std::log10(clamped) - minLog) / (maxLog - minLog), 0.0f, 1.0f);
	}

	static ImU32 stiffnessToColor(float k, uint8_t alpha = 255)
	{
		const float t = stiffnessToNormalizedLog(k);
		struct Stop
		{
			float pos;
			float r;
			float g;
			float b;
		};
		static constexpr Stop stops[] = {
			{0.00f, 80.0f, 145.0f, 255.0f},	 // Very soft: cool blue
			{0.30f, 45.0f, 220.0f, 210.0f},	 // Soft: cyan-teal
			{0.55f, 115.0f, 235.0f, 105.0f}, // Medium: emerald-lime
			{0.78f, 255.0f, 200.0f, 55.0f},	 // Firm: amber-gold
			{1.00f, 255.0f, 85.0f, 60.0f},	 // Stiff: orange-red
		};

		float r = stops[4].r;
		float g = stops[4].g;
		float b = stops[4].b;
		for (size_t i = 0; i + 1 < IM_ARRAYSIZE(stops); ++i)
		{
			if (t <= stops[i + 1].pos)
			{
				const float localT = (t - stops[i].pos) / (std::max)(1e-5f, stops[i + 1].pos - stops[i].pos);
				r = stops[i].r + (stops[i + 1].r - stops[i].r) * localT;
				g = stops[i].g + (stops[i + 1].g - stops[i].g) * localT;
				b = stops[i].b + (stops[i + 1].b - stops[i].b) * localT;
				break;
			}
		}

		return IM_COL32(static_cast<int>(std::round(r)), static_cast<int>(std::round(g)),
						static_cast<int>(std::round(b)), alpha);
	}

	static void formatFloatCompact(char* buf, size_t bufSize, float value, int maxDecimals = 5)
	{
		if (bufSize == 0)
		{
			return;
		}
		std::snprintf(buf, bufSize, "%.*f", maxDecimals, value);
		char* dot = std::strchr(buf, '.');
		if (dot == nullptr)
		{
			return;
		}
		size_t len = std::strlen(buf);
		while (len > static_cast<size_t>(dot - buf) + 3 && buf[len - 1] == '0')
		{
			buf[--len] = '\0';
		}
	}

	static void drawDashedLine(ImDrawList* drawList, ImVec2 a, ImVec2 b, ImU32 color, float thickness,
							   float dashLen = 8.0f, float gapLen = 5.0f)
	{
		const float dx = b.x - a.x;
		const float dy = b.y - a.y;
		const float totalLen = std::sqrt(dx * dx + dy * dy);
		if (totalLen < 1.0f)
		{
			return;
		}
		const float ux = dx / totalLen;
		const float uy = dy / totalLen;
		const float period = dashLen + gapLen;
		for (float d = 0.0f; d < totalLen; d += period)
		{
			const float dEnd = (std::min)(d + dashLen, totalLen);
			drawList->AddLine(ImVec2(a.x + ux * d, a.y + uy * d), ImVec2(a.x + ux * dEnd, a.y + uy * dEnd), color,
							  thickness);
		}
	}

	float getLinkStiffness(const DistanceLink& link) const
	{
		if (link.type == LinkType::Spring && m_tempRegistry != nullptr &&
			m_tempRegistry->hasComponent<Spring>(link.constraintEntity))
		{
			return m_tempRegistry->getComponent<Spring>(link.constraintEntity).stiffness;
		}
		return SPRING_STIFFNESS;
	}

	float getLinkCurrentDistance(const DistanceLink& link) const
	{
		if (m_tempRegistry == nullptr || !hasTransform(link.a) || !hasTransform(link.b))
		{
			return link.restDistance;
		}
		const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
		const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
		return glm::length(vec2(tb.position.x - ta.position.x, tb.position.y - ta.position.y));
	}

	bool hasTransform(Entity e) const
	{
		if (m_tempRegistry == nullptr)
		{
			return false;
		}
		auto arr = m_tempRegistry->getComponentArray<Transform>();
		return arr->hasData(e);
	}

	void renderSceneOverlay()
	{
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();
		drawList->PushClipRect(
			ImVec2(0.0f, m_menuBarHeight),
			ImVec2(static_cast<float>(Display::width) - PANEL_WIDTH, static_cast<float>(Display::height)), true);
		renderGridOverlay(drawList);
		renderLinksOverlay(drawList);
		renderFixedPinsOverlay(drawList);
		renderPendingConstraintOverlay(drawList);
		renderToolHoverOverlay(drawList);
		renderBallDragOverlay(drawList);
		renderRotatePivotOverlay(drawList);
		renderStiffnessLegendOverlay(drawList);
		renderSimulationBannerOverlay(drawList);
		drawList->PopClipRect();
	}

	bool isCtrlHeld() const
	{
		return (m_tempSvc != nullptr && m_tempSvc->input().getKey(Input::LeftCtrl)) || ImGui::GetIO().KeyCtrl;
	}

	void renderBallDragOverlay(ImDrawList* drawList) const
	{
		if (m_dragMode != DragMode::Ball || !isCtrlHeld())
		{
			return;
		}
		const float rightEdge = static_cast<float>(Display::width) - PANEL_WIDTH - 155.0f;
		drawTextPill(drawList, ImVec2((std::max)(12.0f, rightEdge), m_menuBarHeight + 10.0f), IM_COL32(18, 34, 44, 220),
					 IM_COL32(85, 200, 240, 220), IM_COL32(210, 242, 255, 255), "Resize Links (Ctrl)");
	}

	void renderRotatePivotOverlay(ImDrawList* drawList) const
	{
		if (!m_isRotatingDrag || m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}
		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const ImVec2 p = worldToImGuiScreen(cam, m_rotateStartCenter);
		const ImU32 pivotCol = IM_COL32(255, 225, 95, 235);
		drawList->AddCircle(p, 7.0f, pivotCol, 0, 2.0f);
		drawList->AddLine(ImVec2(p.x - 10.0f, p.y), ImVec2(p.x + 10.0f, p.y), pivotCol, 1.5f);
		drawList->AddLine(ImVec2(p.x, p.y - 10.0f), ImVec2(p.x, p.y + 10.0f), pivotCol, 1.5f);

		char angleBuf[32]{};
		std::snprintf(angleBuf, sizeof(angleBuf), "%.1f deg", m_rotateDragAngleDeg);
		drawTextPill(drawList, ImVec2(p.x + 12.0f, p.y - 12.0f), IM_COL32(18, 20, 24, 225), pivotCol,
					 IM_COL32(245, 245, 250, 255), angleBuf);
	}

	void renderGridOverlay(ImDrawList* drawList)
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
			const float c0 =
				(std::min)((std::min)(normal.x * minX + normal.y * minY, normal.x * maxX + normal.y * minY),
						   (std::min)(normal.x * minX + normal.y * maxY, normal.x * maxX + normal.y * maxY));
			const float c1 =
				(std::max)((std::max)(normal.x * minX + normal.y * minY, normal.x * maxX + normal.y * minY),
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

	ImVec2 worldToImGuiScreen(const Transform& cam, vec2 world) const
	{
		const float winH = static_cast<float>((std::max)(1, Display::height));
		const vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, world);
		return ImVec2(screen.x, winH - screen.y);
	}

	ImU32 materialToImColor(uint16_t materialId, uint8_t alpha = 255) const
	{
		if (m_tempSvc == nullptr)
		{
			return IM_COL32(255, 255, 255, alpha);
		}
		const vec4 c = m_tempSvc->materials2D().get(materialId).color;
		const auto toByte = [](float v) -> int
		{ return static_cast<int>((std::clamp)(std::round(v * 255.0f), 0.0f, 255.0f)); };
		return IM_COL32(toByte(c.x), toByte(c.y), toByte(c.z), alpha);
	}

	static void drawTextPill(ImDrawList* drawList, ImVec2 pos, ImU32 bgCol, ImU32 borderCol, ImU32 textCol,
							 const char* text)
	{
		const ImVec2 size = ImGui::CalcTextSize(text);
		const ImVec2 pad(6.0f, 3.0f);
		const ImVec2 pMin(pos.x, pos.y);
		const ImVec2 pMax(pos.x + size.x + pad.x * 2.0f, pos.y + size.y + pad.y * 2.0f);
		drawList->AddRectFilled(pMin, pMax, bgCol, 4.0f);
		drawList->AddRect(pMin, pMax, borderCol, 4.0f);
		drawList->AddText(ImVec2(pMin.x + pad.x, pMin.y + pad.y), textCol, text);
	}

	void renderFixedPinsOverlay(ImDrawList* drawList)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}

		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float r = (std::clamp)(BALL_WORLD_RADIUS / worldPerPixel() * 0.55f, 4.5f, 12.0f);

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity) || !m_tempRegistry->hasComponent<RigidBody2D>(b.entity))
			{
				continue;
			}

			const bool isPinned = (b.entity == m_draggedBall)
									  ? m_keepFixedAfterDrag
									  : m_tempRegistry->getComponent<RigidBody2D>(b.entity).isFixed;
			if (!isPinned)
			{
				continue;
			}

			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			const ImVec2 c = worldToImGuiScreen(cam, vec2(t.position.x, t.position.y));
			const ImVec2 pts[4] = {ImVec2(c.x, c.y - r), ImVec2(c.x + r, c.y), ImVec2(c.x, c.y + r),
								   ImVec2(c.x - r, c.y)};
			drawList->AddPolyline(pts, 4, IM_COL32(255, 220, 60, 235), ImDrawFlags_Closed, 2.0f);
			drawList->AddCircleFilled(c, 2.2f, IM_COL32(255, 220, 60, 240));
		}
	}

	void drawLinkRestExtentTicks(ImDrawList* drawList, ImVec2 sa, ImVec2 sb, float restDist, float worldPerPx,
								 ImU32 color) const
	{
		const float dx = sb.x - sa.x;
		const float dy = sb.y - sa.y;
		const float lenPx = std::sqrt(dx * dx + dy * dy);
		if (lenPx < 2.0f || worldPerPx <= 1e-6f)
		{
			return;
		}

		const float restPx = restDist / worldPerPx;
		if (std::abs(restPx - lenPx) < 3.0f)
		{
			return;
		}

		const float ux = dx / lenPx;
		const float uy = dy / lenPx;
		const float nx = -uy;
		const float ny = ux;
		const ImVec2 mid((sa.x + sb.x) * 0.5f, (sa.y + sb.y) * 0.5f);
		const float halfSpan = restPx * 0.5f;
		const ImVec2 tickA(mid.x - ux * halfSpan, mid.y - uy * halfSpan);
		const ImVec2 tickB(mid.x + ux * halfSpan, mid.y + uy * halfSpan);
		const float tickHalf = 5.5f;

		drawList->AddLine(ImVec2(tickA.x - nx * tickHalf, tickA.y - ny * tickHalf),
						  ImVec2(tickA.x + nx * tickHalf, tickA.y + ny * tickHalf), color, 1.8f);
		drawList->AddLine(ImVec2(tickB.x - nx * tickHalf, tickB.y - ny * tickHalf),
						  ImVec2(tickB.x + nx * tickHalf, tickB.y + ny * tickHalf), color, 1.8f);
	}

	void drawLinkSegment(ImDrawList* drawList, const Transform& cam, const DistanceLink& link, float worldPerPx,
						 bool isModifyMode, bool isSelected, bool isHovered, bool dimUnselected) const
	{
		if (!hasTransform(link.a) || !hasTransform(link.b))
		{
			return;
		}

		const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
		const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
		const ImVec2 sa = worldToImGuiScreen(cam, vec2(ta.position.x, ta.position.y));
		const ImVec2 sb = worldToImGuiScreen(cam, vec2(tb.position.x, tb.position.y));

		const bool isDist = (link.type == LinkType::Distance);
		const float k = isDist ? 1.0f : getLinkStiffness(link);
		float thickness = isDist ? 2.2f : (1.8f + 2.0f * stiffnessToNormalizedLog(k));
		if (isModifyMode)
		{
			thickness += 0.6f;
		}
		if (isSelected)
		{
			thickness += 1.0f;
		}
		if (isHovered)
		{
			thickness += 1.2f;
		}

		const bool dimmed = dimUnselected && !isSelected && !isHovered;
		const uint8_t coreAlpha = dimmed ? 105 : ((isSelected || isHovered) ? 255 : 225);
		const uint8_t casingAlpha = dimmed ? 65 : ((isSelected || isHovered) ? 215 : 145);

		if (isHovered)
		{
			drawList->AddLine(sa, sb, IM_COL32(255, 235, 85, 215), thickness + 7.5f);
		}
		else if (isSelected)
		{
			drawList->AddLine(sa, sb, IM_COL32(240, 248, 255, 165), thickness + 5.5f);
		}

		const ImU32 casingColor = IM_COL32(12, 14, 18, casingAlpha);
		const ImU32 coreColor = isDist ? IM_COL32(190, 232, 255, coreAlpha) : stiffnessToColor(k, coreAlpha);

		if (isDist)
		{
			drawDashedLine(drawList, sa, sb, casingColor, thickness + 1.8f, 9.0f, 5.0f);
			drawDashedLine(drawList, sa, sb, coreColor, thickness, 9.0f, 5.0f);
		}
		else
		{
			drawList->AddLine(sa, sb, casingColor, thickness + 1.8f);
			drawList->AddLine(sa, sb, coreColor, thickness);
		}

		if (isSelected || isHovered)
		{
			const ImU32 capCol = isHovered ? IM_COL32(255, 235, 90, 240) : IM_COL32(245, 250, 255, 235);
			drawList->AddCircleFilled(sa, 3.0f, IM_COL32(12, 14, 18, 220));
			drawList->AddCircleFilled(sb, 3.0f, IM_COL32(12, 14, 18, 220));
			drawList->AddCircleFilled(sa, 2.0f, capCol);
			drawList->AddCircleFilled(sb, 2.0f, capCol);
			drawLinkRestExtentTicks(drawList, sa, sb, link.restDistance, worldPerPx, capCol);
		}
	}

	void renderLinksOverlay(ImDrawList* drawList)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr || m_links.empty())
		{
			return;
		}

		Entity modifyHoverConstraint = static_cast<Entity>(-1);
		if (m_modifyDragActive && isValidEntityId(m_draggedLinkConstraint))
		{
			modifyHoverConstraint = m_draggedLinkConstraint;
		}
		else if (m_toolMode == ToolMode::Modify && m_dragMode == DragMode::None && isMouseInViewport())
		{
			const int idx = pickLinkAtMouse();
			if (idx >= 0)
			{
				modifyHoverConstraint = m_links[static_cast<size_t>(idx)].constraintEntity;
			}
		}

		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float worldPerPx = worldPerPixel();
		const bool isModifyMode = (m_toolMode == ToolMode::Modify);
		const bool hasSelection = !m_selectedLinkConstraints.empty();

		std::vector<size_t> selectedIndices;
		std::vector<size_t> hoveredIndices;

		for (size_t i = 0; i < m_links.size(); ++i)
		{
			const DistanceLink& link = m_links[i];
			const bool isHovered =
				(link.constraintEntity == m_uiHoveredLink) || (link.constraintEntity == modifyHoverConstraint);
			const bool isSelected = isLinkSelected(link.constraintEntity);

			if (isHovered)
			{
				hoveredIndices.push_back(i);
			}
			else if (isSelected)
			{
				selectedIndices.push_back(i);
			}
			else
			{
				drawLinkSegment(drawList, cam, link, worldPerPx, isModifyMode, false, false, hasSelection);
			}
		}

		for (size_t idx : selectedIndices)
		{
			drawLinkSegment(drawList, cam, m_links[idx], worldPerPx, isModifyMode, true, false, hasSelection);
		}
		for (size_t idx : hoveredIndices)
		{
			const DistanceLink& link = m_links[idx];
			drawLinkSegment(drawList, cam, link, worldPerPx, isModifyMode, isLinkSelected(link.constraintEntity), true,
							hasSelection);
		}
	}

	void renderStiffnessLegendOverlay(ImDrawList* drawList) const
	{
		if (m_links.empty())
		{
			return;
		}

		const float winH = static_cast<float>(Display::height);
		const float panelRight = static_cast<float>(Display::width) - PANEL_WIDTH;
		if (winH < m_menuBarHeight + 80.0f || panelRight < 280.0f)
		{
			return;
		}

		const float x0 = 12.0f;
		const float y0 = winH - 38.0f;
		const float w = 260.0f;
		const float h = 26.0f;

		drawList->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), IM_COL32(16, 18, 22, 205), 4.0f);
		drawList->AddRect(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), IM_COL32(75, 82, 95, 190), 4.0f);

		drawList->AddText(ImVec2(x0 + 7.0f, y0 + 6.0f), IM_COL32(200, 208, 220, 235), "k .001");

		const float barX0 = x0 + 50.0f;
		const float barY0 = y0 + 9.0f;
		const float barW = 104.0f;
		const float barH = 8.0f;
		const float segPos[5] = {0.0f, 0.30f, 0.55f, 0.78f, 1.0f};
		const ImU32 segCols[5] = {IM_COL32(80, 145, 255, 255), IM_COL32(45, 220, 210, 255),
								  IM_COL32(115, 235, 105, 255), IM_COL32(255, 200, 55, 255),
								  IM_COL32(255, 85, 60, 255)};

		for (int i = 0; i < 4; ++i)
		{
			const float sx0 = barX0 + barW * segPos[i];
			const float sx1 = barX0 + barW * segPos[i + 1];
			drawList->AddRectFilledMultiColor(ImVec2(sx0, barY0), ImVec2(sx1, barY0 + barH), segCols[i], segCols[i + 1],
											  segCols[i + 1], segCols[i]);
		}
		drawList->AddRect(ImVec2(barX0, barY0), ImVec2(barX0 + barW, barY0 + barH), IM_COL32(30, 34, 40, 220));
		drawList->AddText(ImVec2(barX0 + barW + 5.0f, y0 + 6.0f), IM_COL32(200, 208, 220, 235), "1.0");

		const float dashX0 = barX0 + barW + 34.0f;
		drawDashedLine(drawList, ImVec2(dashX0, y0 + 13.0f), ImVec2(dashX0 + 22.0f, y0 + 13.0f),
					   IM_COL32(190, 232, 255, 245), 2.2f, 5.0f, 3.0f);
		drawList->AddText(ImVec2(dashX0 + 26.0f, y0 + 6.0f), IM_COL32(190, 232, 255, 235), "Dist");
	}

	void renderPendingConstraintOverlay(ImDrawList* drawList)
	{
		if (!isValidEntityId(m_constraintStartBall) || !hasTransform(m_constraintStartBall))
		{
			return;
		}

		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const auto& t = m_tempRegistry->getComponent<Transform>(m_constraintStartBall);
		const vec2 startWorld(t.position.x, t.position.y);
		const ImVec2 center = worldToImGuiScreen(cam, startWorld);
		const ImU32 color =
			(m_toolMode == ToolMode::Distance) ? IM_COL32(85, 215, 255, 225) : IM_COL32(255, 175, 65, 225);
		const float ringRadius = (std::max)(8.0f, BALL_WORLD_RADIUS / worldPerPixel() + 5.0f);

		vec2 endWorld = getMouseWorldPosition();
		ImVec2 endScreen = ImGui::GetIO().MousePos;
		const Entity hoveredTarget = isMouseInViewport() ? pickBallAtMouse() : static_cast<Entity>(-1);
		if (isValidEntityId(hoveredTarget) && hoveredTarget != m_constraintStartBall && hasTransform(hoveredTarget))
		{
			const auto& tb = m_tempRegistry->getComponent<Transform>(hoveredTarget);
			endWorld = vec2(tb.position.x, tb.position.y);
			endScreen = worldToImGuiScreen(cam, endWorld);
		}

		drawList->AddCircle(center, ringRadius, color, 0, 2.5f);
		drawList->AddLine(center, endScreen, color, 2.5f);

		char distBuf[32]{};
		std::snprintf(distBuf, sizeof(distBuf), "d = %.2f", glm::length(endWorld - startWorld));
		const ImVec2 mid((center.x + endScreen.x) * 0.5f + 10.0f, (center.y + endScreen.y) * 0.5f - 12.0f);
		drawTextPill(drawList, mid, IM_COL32(20, 22, 26, 215), color, IM_COL32(240, 240, 245, 255), distBuf);
	}

	void renderToolHoverOverlay(ImDrawList* drawList)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr || m_dragMode != DragMode::None)
		{
			return;
		}

		const bool draggingLink = (m_toolMode == ToolMode::Modify && m_modifyDragActive);
		if (!isMouseInViewport() && !draggingLink)
		{
			return;
		}

		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float ballPx = (std::max)(4.0f, BALL_WORLD_RADIUS / worldPerPixel());
		const ImVec2 mouseScreen = ImGui::GetIO().MousePos;

		switch (m_toolMode)
		{
			case ToolMode::Add:
			{
				const vec2 rawPos = getMouseWorldPosition();
				const vec2 pos = computeNonOverlappingPlacement(rawPos, rawPos, static_cast<Entity>(-1));
				const ImVec2 p = worldToImGuiScreen(cam, pos);
				drawList->AddCircleFilled(p, ballPx, materialToImColor(static_cast<uint16_t>(m_selectedMaterial), 75));
				drawList->AddCircle(p, ballPx, materialToImColor(static_cast<uint16_t>(m_selectedMaterial), 220), 0,
									2.0f);
				break;
			}
			case ToolMode::Spring:
			case ToolMode::Distance:
			{
				const Entity hoverBall = pickBallAtMouse();
				if (isValidEntityId(hoverBall) && hoverBall != m_constraintStartBall && hasTransform(hoverBall))
				{
					const auto& t = m_tempRegistry->getComponent<Transform>(hoverBall);
					const ImVec2 c = worldToImGuiScreen(cam, vec2(t.position.x, t.position.y));
					const ImU32 col =
						(m_toolMode == ToolMode::Distance) ? IM_COL32(85, 215, 255, 225) : IM_COL32(255, 175, 65, 225);
					drawList->AddCircle(c, ballPx + 4.0f, col, 0, 2.5f);
				}
				break;
			}
			case ToolMode::Modify:
			{
				DistanceLink* target = findLinkByConstraint(m_draggedLinkConstraint);
				if (target == nullptr && isMouseInViewport())
				{
					const int idx = pickLinkAtMouse();
					if (idx >= 0)
					{
						target = &m_links[static_cast<size_t>(idx)];
					}
				}
				if (target != nullptr && hasTransform(target->a) && hasTransform(target->b))
				{
					const float k = getLinkStiffness(*target);
					const float curLen = getLinkCurrentDistance(*target);
					const ImU32 col =
						(target->type == LinkType::Distance) ? IM_COL32(190, 232, 255, 245) : stiffnessToColor(k, 255);
					char kBuf[24]{};
					formatFloatCompact(kBuf, sizeof(kBuf), k, 5);
					char label[96]{};
					if (target->type == LinkType::Distance)
					{
						if (std::abs(curLen - target->restDistance) > 0.01f)
						{
							std::snprintf(label, sizeof(label), "Distance  d=%.3f  (cur %.3f)", target->restDistance,
										  curLen);
						}
						else
						{
							std::snprintf(label, sizeof(label), "Distance  d=%.3f", target->restDistance);
						}
					}
					else
					{
						if (std::abs(curLen - target->restDistance) > 0.01f)
						{
							std::snprintf(label, sizeof(label), "Spring  d=%.3f  (cur %.3f)  k=%s",
										  target->restDistance, curLen, kBuf);
						}
						else
						{
							std::snprintf(label, sizeof(label), "Spring  d=%.3f  k=%s", target->restDistance, kBuf);
						}
					}
					drawTextPill(drawList, ImVec2(mouseScreen.x + 14.0f, mouseScreen.y - 24.0f),
								 IM_COL32(18, 20, 24, 232), col, IM_COL32(245, 245, 250, 255), label);
				}
				break;
			}
			case ToolMode::Remove:
			{
				if (m_removeTarget == RemoveTarget::Auto && m_tempSvc->input().getKey(Input::LeftShift))
				{
					const float rightEdge = static_cast<float>(Display::width) - PANEL_WIDTH - 135.0f;
					drawTextPill(drawList, ImVec2((std::max)(12.0f, rightEdge), m_menuBarHeight + 10.0f),
								 IM_COL32(40, 18, 18, 220), IM_COL32(255, 90, 90, 220), IM_COL32(255, 195, 195, 255),
								 "Links Only (Shift)");
				}

				Entity hitBall = static_cast<Entity>(-1);
				int hitLinkIdx = -1;
				resolveRemoveTarget(hitBall, hitLinkIdx);

				if (isValidEntityId(hitBall) && hasTransform(hitBall))
				{
					const auto& t = m_tempRegistry->getComponent<Transform>(hitBall);
					const ImVec2 c = worldToImGuiScreen(cam, vec2(t.position.x, t.position.y));
					const ImU32 red = IM_COL32(255, 70, 70, 240);
					drawList->AddCircle(c, ballPx + 4.0f, red, 0, 2.8f);
					const float xSz = (std::clamp)(ballPx * 0.45f, 4.0f, 10.0f);
					drawList->AddLine(ImVec2(c.x - xSz, c.y - xSz), ImVec2(c.x + xSz, c.y + xSz), red, 2.2f);
					drawList->AddLine(ImVec2(c.x - xSz, c.y + xSz), ImVec2(c.x + xSz, c.y - xSz), red, 2.2f);
					drawTextPill(drawList, ImVec2(mouseScreen.x + 14.0f, mouseScreen.y - 22.0f),
								 IM_COL32(36, 16, 16, 225), red, IM_COL32(255, 220, 220, 255), "Delete Ball");
				}
				else if (hitLinkIdx >= 0 && hitLinkIdx < static_cast<int>(m_links.size()))
				{
					const DistanceLink& link = m_links[static_cast<size_t>(hitLinkIdx)];
					if (hasTransform(link.a) && hasTransform(link.b))
					{
						const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
						const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
						const ImVec2 sa = worldToImGuiScreen(cam, vec2(ta.position.x, ta.position.y));
						const ImVec2 sb = worldToImGuiScreen(cam, vec2(tb.position.x, tb.position.y));
						const ImVec2 mid((sa.x + sb.x) * 0.5f, (sa.y + sb.y) * 0.5f);
						const ImU32 red = IM_COL32(255, 70, 70, 240);
						drawList->AddLine(sa, sb, red, 7.5f);
						drawList->AddCircleFilled(mid, 5.0f, red);
						const char* kind = (link.type == LinkType::Distance) ? "Delete Distance" : "Delete Spring";
						drawTextPill(drawList, ImVec2(mouseScreen.x + 14.0f, mouseScreen.y - 22.0f),
									 IM_COL32(36, 16, 16, 225), red, IM_COL32(255, 220, 220, 255), kind);
					}
				}
				break;
			}
			case ToolMode::TagEditor:
			{
				const Entity hoverBall = pickBallAtMouse();
				if (isValidEntityId(hoverBall) && hoverBall != m_tagSelectedEntity && hasTransform(hoverBall))
				{
					const auto& t = m_tempRegistry->getComponent<Transform>(hoverBall);
					const ImVec2 c = worldToImGuiScreen(cam, vec2(t.position.x, t.position.y));
					drawList->AddCircle(c, ballPx + 4.0f, IM_COL32(110, 220, 235, 220), 0, 2.2f);
				}
				break;
			}
			case ToolMode::Material:
			{
				const Entity hoverBall = pickBallAtMouse();
				if (isValidEntityId(hoverBall) && hasTransform(hoverBall))
				{
					const auto& t = m_tempRegistry->getComponent<Transform>(hoverBall);
					const ImVec2 c = worldToImGuiScreen(cam, vec2(t.position.x, t.position.y));
					drawList->AddCircle(c, ballPx + 4.0f,
										materialToImColor(static_cast<uint16_t>(m_selectedMaterial), 235), 0, 2.6f);
				}
				break;
			}
		}
	}

	void renderSimulationBannerOverlay(ImDrawList* drawList)
	{
		if (!m_isSimulating)
		{
			return;
		}
		const char* text = m_hasPlaySnapshot ? "SIMULATING  |  Space: Pause & Rewind"
											 : "SIMULATING (Edited)  |  Space: Pause (Edits Kept)";
		drawTextPill(drawList, ImVec2(12.0f, m_menuBarHeight + 10.0f), IM_COL32(45, 26, 14, 225),
					 IM_COL32(235, 130, 50, 235), IM_COL32(255, 230, 200, 255), text);
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

	std::string ballLabel(Entity entity, SimulationID simId) const
	{
		if (m_tempSvc != nullptr && m_tempSvc->names().hasName(entity))
		{
			return m_tempSvc->names().getEntityName(entity);
		}
		if (simId != INVALID_SIMULATION_ID)
		{
			return "#" + std::to_string(simId.id);
		}
		return "e" + std::to_string(static_cast<unsigned int>(entity));
	}

	void rebuildLinkConstraint(DistanceLink& link, LinkType newType, float springStiffness)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr || !hasTransform(link.a) || !hasTransform(link.b))
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();

		const Entity oldConstraintEnt = link.constraintEntity;
		if (m_draggedLinkConstraint == oldConstraintEnt)
		{
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			m_hasPendingLinkDragUndo = false;
		}
		if (m_uiHoveredLink == oldConstraintEnt)
		{
			m_uiHoveredLink = static_cast<Entity>(-1);
		}

		m_tempRegistry->destroyEntity(oldConstraintEnt);
		m_tempRegistry->freeRemovedComponents();

		link.restDistance = (std::max)(link.restDistance, minAllowedLinkDistance(newType));
		Entity constraintEnt =
			createConstraintComponentEntity(link.a, link.b, newType, link.restDistance, springStiffness);
		if (constraintEnt == INVALID_ENTITY)
		{
			return;
		}

		for (Entity& selected : m_selectedLinkConstraints)
		{
			if (selected == oldConstraintEnt)
			{
				selected = constraintEnt;
			}
		}
		if (m_modifyPressedLink == oldConstraintEnt)
		{
			m_modifyPressedLink = constraintEnt;
		}

		link.type = newType;
		link.constraintEntity = constraintEnt;
	}

	bool isLinkSelected(Entity constraintEntity) const
	{
		if (!isValidEntityId(constraintEntity))
		{
			return false;
		}
		return std::find(m_selectedLinkConstraints.begin(), m_selectedLinkConstraints.end(), constraintEntity) !=
			   m_selectedLinkConstraints.end();
	}

	void clearLinkSelection()
	{
		m_selectedLinkConstraints.clear();
		m_syncedLinkPrimary = static_cast<Entity>(-1);
		m_syncedLinkSelCount = 0;
	}

	void selectSingleLink(Entity constraintEntity)
	{
		clearLinkSelection();
		if (isValidEntityId(constraintEntity) && findLinkByConstraint(constraintEntity) != nullptr)
		{
			m_selectedLinkConstraints.push_back(constraintEntity);
		}
	}

	void toggleLinkSelection(Entity constraintEntity)
	{
		if (!isValidEntityId(constraintEntity))
		{
			return;
		}
		auto it = std::find(m_selectedLinkConstraints.begin(), m_selectedLinkConstraints.end(), constraintEntity);
		if (it != m_selectedLinkConstraints.end())
		{
			m_selectedLinkConstraints.erase(it);
		}
		else if (findLinkByConstraint(constraintEntity) != nullptr)
		{
			m_selectedLinkConstraints.push_back(constraintEntity);
		}
	}

	void pruneLinkSelection()
	{
		m_selectedLinkConstraints.erase(
			std::remove_if(m_selectedLinkConstraints.begin(), m_selectedLinkConstraints.end(), [this](Entity ent)
						   { return !isValidEntityId(ent) || findLinkByConstraint(ent) == nullptr; }),
			m_selectedLinkConstraints.end());
	}

	std::vector<size_t> collectSelectedLinkIndices()
	{
		pruneLinkSelection();
		std::vector<size_t> indices;
		indices.reserve(m_selectedLinkConstraints.size());
		for (size_t i = 0; i < m_links.size(); ++i)
		{
			if (isLinkSelected(m_links[i].constraintEntity))
			{
				indices.push_back(i);
			}
		}
		return indices;
	}

	void selectLinksByFilter(LinkFilterMode filter)
	{
		clearLinkSelection();
		for (const DistanceLink& link : m_links)
		{
			if (filter == LinkFilterMode::All || (filter == LinkFilterMode::Springs && link.type == LinkType::Spring) ||
				(filter == LinkFilterMode::Distance && link.type == LinkType::Distance))
			{
				m_selectedLinkConstraints.push_back(link.constraintEntity);
			}
		}
	}

	void selectSpringsBySimilarStiffness(float referenceK)
	{
		clearLinkSelection();
		const float tol = (std::max)(0.0005f, referenceK * 0.12f);
		for (const DistanceLink& link : m_links)
		{
			if (link.type == LinkType::Spring && std::abs(getLinkStiffness(link) - referenceK) <= tol)
			{
				m_selectedLinkConstraints.push_back(link.constraintEntity);
			}
		}
	}

	void deleteSelectedLinks()
	{
		std::vector<size_t> indices = collectSelectedLinkIndices();
		if (indices.empty() || m_tempRegistry == nullptr)
		{
			return;
		}
		pushUndoState();
		for (auto it = indices.rbegin(); it != indices.rend(); ++it)
		{
			removeLinkAt(*it);
		}
		m_tempRegistry->freeRemovedComponents();
		clearLinkSelection();
	}

	void renderTagSection()
	{
		if (!ImGui::CollapsingHeader("Selected Ball", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		if (!isValidEntityId(m_tagSelectedEntity) || !hasTransform(m_tagSelectedEntity))
		{
			m_tagSelectedEntity = static_cast<Entity>(-1);
			ImGui::TextWrapped("No ball selected. Pick Tag [6] and left-click a ball to select it.");
			return;
		}

		if (m_tagBufferEntity != m_tagSelectedEntity)
		{
			m_tagBufferEntity = m_tagSelectedEntity;
			const std::string currentName = m_tempSvc->names().getEntityName(m_tagSelectedEntity);
			std::snprintf(m_tagBuf, sizeof(m_tagBuf), "%s", currentName.c_str());
			m_newTagBuf[0] = '\0';
		}

		const SimulationID simId = getSimulationId(m_tagSelectedEntity);
		ImGui::Text("Ball: entity %u (#%u)", static_cast<unsigned int>(m_tagSelectedEntity), simId.id);

		if (m_tempRegistry->hasComponent<RigidBody2D>(m_tagSelectedEntity))
		{
			auto& rb = m_tempRegistry->getComponent<RigidBody2D>(m_tagSelectedEntity);
			bool pinned = rb.isFixed;
			if (ImGui::Checkbox("Pinned (Fixed) [F]", &pinned))
			{
				pushUndoState();
				rb.isFixed = pinned;
				m_tempRegistry->setComponentDirty(rb);
			}
		}

		if (m_tempRegistry->hasComponent<Dot>(m_tagSelectedEntity))
		{
			auto& dot = m_tempRegistry->getComponent<Dot>(m_tagSelectedEntity);
			const vec4 col = m_tempSvc->materials2D().get(static_cast<uint16_t>(dot.materialId)).color;
			ImGui::ColorButton("##selectedMatSwatch", ImVec4(col.x, col.y, col.z, 1.0f),
							   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18.0f, 18.0f));
			ImGui::SameLine();
			ImGui::Text("Material %u", dot.materialId);
			ImGui::SameLine();
			if (ImGui::SmallButton("Apply Selected"))
			{
				const unsigned int nextMat = static_cast<unsigned int>(m_selectedMaterial);
				if (dot.materialId != nextMat)
				{
					pushUndoState();
					dot.materialId = nextMat;
				}
			}
		}

		// --- Unique Local Name ---
		ImGui::SeparatorText("Unique Name");
		ImGui::SetNextItemWidth(-1.0f);
		const bool submittedName = ImGui::InputTextWithHint("##name", "name (e.g. head, hand)", m_tagBuf,
															sizeof(m_tagBuf), ImGuiInputTextFlags_EnterReturnsTrue);

		const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		const bool applyName = ImGui::Button("Apply Name", ImVec2(buttonWidth, 0.0f)) || submittedName;
		ImGui::SameLine();
		const bool removeName = ImGui::Button("Clear Name", ImVec2(buttonWidth, 0.0f));

		if (applyName)
		{
			const std::string newName(m_tagBuf);
			const std::string oldName = m_tempSvc->names().getEntityName(m_tagSelectedEntity);
			if (newName != oldName)
			{
				pushUndoState();
				if (newName.empty())
				{
					m_tempSvc->names().removeName(m_tagSelectedEntity);
				}
				else
				{
					m_tempSvc->names().setName(m_tagSelectedEntity, newName);
				}
			}
		}

		if (removeName)
		{
			if (m_tempSvc->names().hasName(m_tagSelectedEntity))
			{
				pushUndoState();
				m_tempSvc->names().removeName(m_tagSelectedEntity);
			}
			m_tagBuf[0] = '\0';
		}

		// --- Multi-Tags ---
		ImGui::SeparatorText("Tags");
		const auto& currentTags = m_tempSvc->tags().getTags(m_tagSelectedEntity);
		if (currentTags.empty())
		{
			ImGui::TextDisabled("No tags attached");
		}
		else
		{
			std::string tagToRemove;
			for (const auto& tag : currentTags)
			{
				ImGui::BulletText("%s", tag.c_str());
				ImGui::SameLine();
				std::string btnId = "X##" + tag;
				if (ImGui::SmallButton(btnId.c_str()))
				{
					tagToRemove = tag;
				}
			}
			if (!tagToRemove.empty())
			{
				pushUndoState();
				m_tempSvc->tags().removeTag(m_tagSelectedEntity, tagToRemove);
			}
		}

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
		const bool submittedTag = ImGui::InputTextWithHint("##newTag", "add tag...", m_newTagBuf, sizeof(m_newTagBuf),
														   ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::SameLine();
		if ((ImGui::Button("Add Tag", ImVec2(-1.0f, 0.0f)) || submittedTag) && m_newTagBuf[0] != '\0')
		{
			const std::string newTag(m_newTagBuf);
			const auto& tagsNow = m_tempSvc->tags().getTags(m_tagSelectedEntity);
			if (std::find(tagsNow.begin(), tagsNow.end(), newTag) == tagsNow.end())
			{
				pushUndoState();
				m_tempSvc->tags().addTag(m_tagSelectedEntity, newTag);
			}
			m_newTagBuf[0] = '\0';
		}

		ImGui::Spacing();
		const float actionBtnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button("Delete Ball", ImVec2(actionBtnWidth, 0.0f)))
		{
			pushUndoState();
			removeBall(m_tagSelectedEntity);
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button("Deselect", ImVec2(actionBtnWidth, 0.0f)))
		{
			clearTagSelection();
		}
	}

	void renderSelectedLinkSection()
	{
		if (!ImGui::CollapsingHeader("Selected Link(s)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		std::vector<size_t> selectedIndices = collectSelectedLinkIndices();
		size_t springCount = 0;
		size_t distCount = 0;
		float firstDist = 0.0f;
		float minDist = 0.0f;
		float maxDist = 0.0f;
		float firstK = SPRING_STIFFNESS;
		float minK = STIFFNESS_MAX;
		float maxK = STIFFNESS_MIN;

		for (size_t i = 0; i < selectedIndices.size(); ++i)
		{
			const DistanceLink& link = m_links[selectedIndices[i]];
			if (i == 0)
			{
				firstDist = link.restDistance;
				minDist = link.restDistance;
				maxDist = link.restDistance;
			}
			else
			{
				minDist = (std::min)(minDist, link.restDistance);
				maxDist = (std::max)(maxDist, link.restDistance);
			}

			if (link.type == LinkType::Spring)
			{
				const float k = getLinkStiffness(link);
				if (springCount == 0)
				{
					firstK = k;
					minK = k;
					maxK = k;
				}
				else
				{
					minK = (std::min)(minK, k);
					maxK = (std::max)(maxK, k);
				}
				++springCount;
			}
			else
			{
				++distCount;
			}
		}

		const bool hasLinks = !m_links.empty();
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float batchW = (ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f;

		if (!hasLinks)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Springs", ImVec2(batchW, 20.0f)))
		{
			selectLinksByFilter(LinkFilterMode::Springs);
			selectedIndices = collectSelectedLinkIndices();
		}
		ImGui::SameLine();
		if (ImGui::Button("Dist", ImVec2(batchW, 20.0f)))
		{
			selectLinksByFilter(LinkFilterMode::Distance);
			selectedIndices = collectSelectedLinkIndices();
		}
		ImGui::SameLine();
		const bool canSameK = (springCount > 0);
		if (!canSameK)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Same k", ImVec2(batchW, 20.0f)))
		{
			selectSpringsBySimilarStiffness(firstK);
			selectedIndices = collectSelectedLinkIndices();
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Select all springs with matching stiffness k");
		}
		if (!canSameK)
		{
			ImGui::EndDisabled();
		}
		ImGui::SameLine();
		const bool hasSel = !selectedIndices.empty();
		if (!hasSel)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Clear", ImVec2(batchW, 20.0f)))
		{
			clearLinkSelection();
			selectedIndices.clear();
		}
		if (!hasSel)
		{
			ImGui::EndDisabled();
		}
		if (!hasLinks)
		{
			ImGui::EndDisabled();
		}

		if (selectedIndices.empty())
		{
			ImGui::TextWrapped("No links selected. Pick Modify [4] and click any link (Shift+Click to multi-select), "
							   "or click a row below.");
			return;
		}

		// Sync text edit buffers whenever selection or external drag values change.
		const Entity primaryConstraint = m_links[selectedIndices[0]].constraintEntity;
		static bool s_distTextActive = false;
		static bool s_stiffTextActive = false;
		const bool selChanged =
			(m_syncedLinkPrimary != primaryConstraint) || (m_syncedLinkSelCount != selectedIndices.size());
		if (!s_distTextActive && (selChanged || std::abs(m_syncedLinkDist - firstDist) > 1e-4f))
		{
			formatFloatCompact(m_linkDistInputBuf, sizeof(m_linkDistInputBuf), firstDist, 3);
			m_syncedLinkDist = firstDist;
		}
		if (!s_stiffTextActive && (selChanged || std::abs(m_syncedLinkK - firstK) > 1e-6f))
		{
			formatFloatCompact(m_linkStiffInputBuf, sizeof(m_linkStiffInputBuf), firstK, 5);
			m_syncedLinkK = firstK;
		}
		m_syncedLinkPrimary = primaryConstraint;
		m_syncedLinkSelCount = selectedIndices.size();

		if (selectedIndices.size() == 1)
		{
			const DistanceLink& only = m_links[selectedIndices[0]];
			const std::string labelA = ballLabel(only.a, only.simulationIdA);
			const std::string labelB = ballLabel(only.b, only.simulationIdB);
			const float curLen = getLinkCurrentDistance(only);
			ImGui::Text("%s <-> %s", labelA.c_str(), labelB.c_str());
			ImGui::SameLine();
			ImGui::TextDisabled("(cur %.3f)", curLen);
		}
		else
		{
			ImGui::Text("%zu links (%zu Spring, %zu Dist)", selectedIndices.size(), springCount, distCount);
		}

		// Type switch across all selected links.
		const float halfW = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
		const bool allSprings = (springCount == selectedIndices.size());
		const bool allDists = (distCount == selectedIndices.size());
		if (allSprings)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.42f, 0.14f, 1.0f));
		}
		if (ImGui::Button("Spring", ImVec2(halfW, 22.0f)) && !allSprings)
		{
			pushUndoState();
			const float targetK = (springCount > 0) ? firstK : SPRING_STIFFNESS;
			for (size_t idx : selectedIndices)
			{
				if (m_links[idx].type != LinkType::Spring)
				{
					rebuildLinkConstraint(m_links[idx], LinkType::Spring, targetK);
				}
			}
			m_syncedLinkK = -1.0f;
		}
		if (allSprings)
		{
			ImGui::PopStyleColor();
		}
		ImGui::SameLine();
		if (allDists)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.65f, 1.0f));
		}
		if (ImGui::Button("Distance", ImVec2(halfW, 22.0f)) && !allDists)
		{
			pushUndoState();
			for (size_t idx : selectedIndices)
			{
				if (m_links[idx].type != LinkType::Distance)
				{
					rebuildLinkConstraint(m_links[idx], LinkType::Distance, SPRING_STIFFNESS);
				}
			}
		}
		if (allDists)
		{
			ImGui::PopStyleColor();
		}

		// Rest Distance exact editing
		ImGui::Spacing();
		const bool distUniform = (maxDist - minDist) <= 0.001f;
		if (!distUniform)
		{
			ImGui::Text("Rest Distance (mixed %.2f .. %.2f)", minDist, maxDist);
		}
		else
		{
			ImGui::Text("Rest Distance");
		}

		const float dragMinDist = (springCount == 0) ? BALL_DIAMETER : MIN_LINK_DISTANCE;
		float dragDist = firstDist;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat("##selDistDrag", &dragDist, 0.01f, dragMinDist, MAX_LINK_DISTANCE,
							 distUniform ? "%.3f" : "~ %.3f"))
		{
			if (!m_hasUiDragUndo)
			{
				m_uiDragUndoState = captureUndoState();
				m_hasUiDragUndo = true;
			}
			for (size_t idx : selectedIndices)
			{
				m_links[idx].restDistance =
					(std::clamp)(dragDist, minAllowedLinkDistance(m_links[idx].type), MAX_LINK_DISTANCE);
				applyLinkDistance(m_links[idx]);
			}
		}
		if (ImGui::IsItemDeactivatedAfterEdit() && m_hasUiDragUndo)
		{
			commitUndoState(m_uiDragUndoState);
			m_hasUiDragUndo = false;
		}

		const float exactW = (std::max)(80.0f, ImGui::GetContentRegionAvail().x - 95.0f - spacing);
		ImGui::SetNextItemWidth(exactW);
		const bool distEnter =
			ImGui::InputTextWithHint("##selDistInput", "exact dist", m_linkDistInputBuf, sizeof(m_linkDistInputBuf),
									 ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
		s_distTextActive = ImGui::IsItemActive();
		const bool distBlurCommit = ImGui::IsItemDeactivatedAfterEdit();
		if (distEnter || distBlurCommit)
		{
			char* endPtr = nullptr;
			const float parsed = std::strtof(m_linkDistInputBuf, &endPtr);
			if (endPtr != m_linkDistInputBuf && std::isfinite(parsed))
			{
				pushUndoState();
				for (size_t idx : selectedIndices)
				{
					m_links[idx].restDistance =
						(std::clamp)(parsed, minAllowedLinkDistance(m_links[idx].type), MAX_LINK_DISTANCE);
					applyLinkDistance(m_links[idx]);
				}
				m_syncedLinkDist = -1.0f;
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Match Cur", ImVec2(-1.0f, 0.0f)))
		{
			pushUndoState();
			for (size_t idx : selectedIndices)
			{
				resizeLinkToCurrentDistance(m_links[idx]);
			}
			m_syncedLinkDist = -1.0f;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Set rest distance to current distance between endpoints");
		}

		// Fine distance nudges
		const float nudgeVals[4] = {-0.1f, -0.01f, 0.01f, 0.1f};
		const char* nudgeLabels[4] = {"-0.1", "-0.01", "+0.01", "+0.1"};
		const float nudgeW = (ImGui::GetContentRegionAvail().x - spacing * 3.0f) * 0.25f;
		for (int i = 0; i < 4; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			if (ImGui::Button(nudgeLabels[i], ImVec2(nudgeW, 20.0f)))
			{
				pushUndoState();
				for (size_t idx : selectedIndices)
				{
					m_links[idx].restDistance =
						(std::clamp)(m_links[idx].restDistance + nudgeVals[i],
									 minAllowedLinkDistance(m_links[idx].type), MAX_LINK_DISTANCE);
					applyLinkDistance(m_links[idx]);
				}
				m_syncedLinkDist = -1.0f;
			}
		}

		// Exact Stiffness controls (when springs are selected)
		if (springCount > 0)
		{
			ImGui::Spacing();
			const ImU32 kColU32 = stiffnessToColor(firstK, 255);
			const ImVec4 kColVec = ImGui::ColorConvertU32ToFloat4(kColU32);
			ImGui::ColorButton("##kSwatch", kColVec, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
							   ImVec2(14.0f, 14.0f));
			ImGui::SameLine();
			const bool kUniform = (maxK - minK) <= 1e-5f;
			if (!kUniform)
			{
				ImGui::Text("Stiffness k (%.4f .. %.4f)", minK, maxK);
			}
			else
			{
				ImGui::Text("Stiffness (k)");
			}

			float sliderK = firstK;
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::SliderFloat("##selKLog", &sliderK, STIFFNESS_MIN, STIFFNESS_MAX, kUniform ? "%.4f" : "~ %.4f",
								   ImGuiSliderFlags_Logarithmic))
			{
				const float clampedK = (std::clamp)(sliderK, STIFFNESS_MIN, STIFFNESS_MAX);
				if (std::abs(clampedK - firstK) > 1e-6f || !kUniform)
				{
					if (!m_hasUiDragUndo)
					{
						m_uiDragUndoState = captureUndoState();
						m_hasUiDragUndo = true;
					}
					for (size_t idx : selectedIndices)
					{
						if (m_links[idx].type == LinkType::Spring)
						{
							rebuildLinkConstraint(m_links[idx], LinkType::Spring, clampedK);
						}
					}
				}
			}
			if (ImGui::IsItemDeactivatedAfterEdit() && m_hasUiDragUndo)
			{
				commitUndoState(m_uiDragUndoState);
				m_hasUiDragUndo = false;
			}

			ImGui::SetNextItemWidth(-1.0f);
			const bool stiffEnter = ImGui::InputTextWithHint(
				"##selKInput", "exact k (0.001 - 1.0, Enter)", m_linkStiffInputBuf, sizeof(m_linkStiffInputBuf),
				ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_EnterReturnsTrue);
			s_stiffTextActive = ImGui::IsItemActive();
			const bool stiffBlurCommit = ImGui::IsItemDeactivatedAfterEdit();
			if (stiffEnter || stiffBlurCommit)
			{
				char* endPtr = nullptr;
				const float parsed = std::strtof(m_linkStiffInputBuf, &endPtr);
				if (endPtr != m_linkStiffInputBuf && std::isfinite(parsed))
				{
					const float clampedK = (std::clamp)(parsed, STIFFNESS_MIN, STIFFNESS_MAX);
					pushUndoState();
					for (size_t idx : selectedIndices)
					{
						if (m_links[idx].type == LinkType::Spring)
						{
							rebuildLinkConstraint(m_links[idx], LinkType::Spring, clampedK);
						}
					}
					m_syncedLinkK = -1.0f;
				}
			}

			const float presets[6] = {0.002f, 0.01f, 0.05f, 0.15f, 0.5f, 1.0f};
			const char* presetLabels[6] = {"0.002", "0.01", "0.05", "0.15", "0.5", "1.0"};
			const float presetW = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
			for (int i = 0; i < 6; ++i)
			{
				if (i % 3 != 0)
				{
					ImGui::SameLine();
				}
				const bool match = kUniform && std::abs(firstK - presets[i]) <= presets[i] * 0.04f;
				if (match)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.52f, 0.62f, 1.0f));
				}
				if (ImGui::Button(presetLabels[i], ImVec2(presetW, 20.0f)))
				{
					pushUndoState();
					for (size_t idx : selectedIndices)
					{
						if (m_links[idx].type == LinkType::Spring)
						{
							rebuildLinkConstraint(m_links[idx], LinkType::Spring, presets[i]);
						}
					}
					m_syncedLinkK = -1.0f;
				}
				if (match)
				{
					ImGui::PopStyleColor();
				}
			}
		}

		if (m_hasUiDragUndo && !ImGui::IsAnyItemActive())
		{
			commitUndoState(m_uiDragUndoState);
			m_hasUiDragUndo = false;
		}

		ImGui::Spacing();
		char delLabel[48]{};
		std::snprintf(delLabel, sizeof(delLabel), "Delete Selected (%zu)", selectedIndices.size());
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.2f, 0.2f, 1.0f));
		if (ImGui::Button(delLabel, ImVec2(-1.0f, 22.0f)))
		{
			deleteSelectedLinks();
		}
		ImGui::PopStyleColor();
	}

	void renderLinksSection()
	{
		m_uiHoveredLink = static_cast<Entity>(-1);

		if (!ImGui::CollapsingHeader("Links", ImGuiTreeNodeFlags_DefaultOpen))
		{
			return;
		}

		size_t springCount = 0;
		for (const DistanceLink& link : m_links)
		{
			if (link.type == LinkType::Spring)
			{
				++springCount;
			}
		}
		const size_t distCount = m_links.size() - springCount;
		ImGui::TextDisabled("%zu total (%zu S, %zu D)", m_links.size(), springCount, distCount);
		if (m_links.empty())
		{
			return;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Select Shown"))
		{
			selectLinksByFilter(m_linkFilter);
		}

		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float thirdW = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
		const char* filterLabels[3] = {"All", "Springs", "Distance"};
		const LinkFilterMode filterModes[3] = {LinkFilterMode::All, LinkFilterMode::Springs, LinkFilterMode::Distance};
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			const bool active = (m_linkFilter == filterModes[i]);
			if (active)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.48f, 0.58f, 1.0f));
			}
			if (ImGui::Button(filterLabels[i], ImVec2(thirdW, 20.0f)))
			{
				m_linkFilter = filterModes[i];
			}
			if (active)
			{
				ImGui::PopStyleColor();
			}
		}

		int removeIndex = -1;
		int clickedIndex = -1;

		ImGui::BeginChild("##LinksList", ImVec2(0.0f, 175.0f), true);
		for (int i = 0; i < static_cast<int>(m_links.size()); ++i)
		{
			const DistanceLink& link = m_links[static_cast<size_t>(i)];
			if (m_linkFilter == LinkFilterMode::Springs && link.type != LinkType::Spring)
			{
				continue;
			}
			if (m_linkFilter == LinkFilterMode::Distance && link.type != LinkType::Distance)
			{
				continue;
			}

			ImGui::PushID(i);
			bool rowHovered = false;
			const bool isSelected = isLinkSelected(link.constraintEntity);
			const bool isDist = (link.type == LinkType::Distance);
			const float k = isDist ? 1.0f : getLinkStiffness(link);
			const ImU32 dotU32 = isDist ? IM_COL32(190, 232, 255, 255) : stiffnessToColor(k, 255);

			const float availW = ImGui::GetContentRegionAvail().x;
			const float deleteBtnW = 20.0f;
			const float rowW = (std::max)(80.0f, availW - deleteBtnW - spacing);

			const std::string labelA = ballLabel(link.a, link.simulationIdA);
			const std::string labelB = ballLabel(link.b, link.simulationIdB);
			char rowText[96]{};
			if (isDist)
			{
				std::snprintf(rowText, sizeof(rowText), "  %s<->%s  d=%.2f [D]", labelA.c_str(), labelB.c_str(),
							  link.restDistance);
			}
			else
			{
				char kBuf[16]{};
				formatFloatCompact(kBuf, sizeof(kBuf), k, 4);
				std::snprintf(rowText, sizeof(rowText), "  %s<->%s  d=%.2f k=%s", labelA.c_str(), labelB.c_str(),
							  link.restDistance, kBuf);
			}

			const ImVec2 cursorScreen = ImGui::GetCursorScreenPos();
			if (ImGui::Selectable(rowText, isSelected, 0, ImVec2(rowW, 18.0f)))
			{
				clickedIndex = i;
			}
			rowHovered |= ImGui::IsItemHovered();

			ImDrawList* listDraw = ImGui::GetWindowDrawList();
			listDraw->AddCircleFilled(ImVec2(cursorScreen.x + 5.0f, cursorScreen.y + 9.0f), 4.0f, dotU32);

			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.2f, 0.2f, 0.85f));
			if (ImGui::SmallButton("X"))
			{
				removeIndex = i;
			}
			rowHovered |= ImGui::IsItemHovered();
			ImGui::PopStyleColor();

			if (rowHovered)
			{
				m_uiHoveredLink = link.constraintEntity;
			}

			ImGui::PopID();
		}
		ImGui::EndChild();

		if (clickedIndex >= 0 && clickedIndex < static_cast<int>(m_links.size()))
		{
			const Entity ent = m_links[static_cast<size_t>(clickedIndex)].constraintEntity;
			if (ImGui::GetIO().KeyShift)
			{
				toggleLinkSelection(ent);
			}
			else
			{
				selectSingleLink(ent);
			}
		}

		if (removeIndex >= 0)
		{
			pushUndoState();
			removeLinkAt(static_cast<size_t>(removeIndex));
			m_tempRegistry->freeRemovedComponents();
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
		ImGui::InputTextWithHint("##filename", "cluster.weirdcluster", m_fileNameBuf, sizeof(m_fileNameBuf));

		const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
		if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
		{
			saveClusterFromName();
		}
		ImGui::SameLine();
		if (ImGui::Button("Load", ImVec2(buttonWidth, 0.0f)))
		{
			loadClusterFromName();
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

	void focusCameraOn(vec2 worldCenter, float distanceZ)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}
		const int winW = Display::width > 0 ? Display::width : 1280;
		const int winH = Display::height > 0 ? Display::height : 800;

		auto& camTransform = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		camTransform.position.z = (std::clamp)(distanceZ, CAMERA_MIN_DISTANCE, CAMERA_MAX_DISTANCE);

		const float halfW = static_cast<float>(winW) * 0.5f;
		const float halfH = static_cast<float>(winH) * 0.5f;
		const float viewCenterX = (static_cast<float>(winW) - PANEL_WIDTH) * 0.5f;
		const float viewCenterY = m_menuBarHeight + (static_cast<float>(winH) - m_menuBarHeight) * 0.5f;
		const float bottomUpCenterY = static_cast<float>(winH) - viewCenterY;
		const float scale = camTransform.position.z / halfH;

		camTransform.position.x = worldCenter.x + (halfW - viewCenterX) * scale;
		camTransform.position.y = worldCenter.y + (halfH - bottomUpCenterY) * scale;
		m_tempRegistry->setComponentDirty(camTransform);
		g_cameraPositon = camTransform.position;
	}

	void centerCameraOnSceneView()
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}
		const auto& camTransform = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		focusCameraOn(vec2(0.0f), camTransform.position.z);
	}

	void frameCluster()
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}
		if (m_balls.empty())
		{
			centerCameraOnSceneView();
			return;
		}

		vec2 minPos((std::numeric_limits<float>::max)());
		vec2 maxPos(std::numeric_limits<float>::lowest());
		bool anyValid = false;

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
			{
				continue;
			}
			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			minPos.x = (std::min)(minPos.x, t.position.x);
			minPos.y = (std::min)(minPos.y, t.position.y);
			maxPos.x = (std::max)(maxPos.x, t.position.x);
			maxPos.y = (std::max)(maxPos.y, t.position.y);
			anyValid = true;
		}

		if (!anyValid)
		{
			centerCameraOnSceneView();
			return;
		}

		const int winW = Display::width > 0 ? Display::width : 1280;
		const int winH = Display::height > 0 ? Display::height : 800;
		const float halfH = static_cast<float>(winH) * 0.5f;
		const float viewW = (std::max)(120.0f, static_cast<float>(winW) - PANEL_WIDTH);
		const float viewH = (std::max)(120.0f, static_cast<float>(winH) - m_menuBarHeight);

		const float spanX = (maxPos.x - minPos.x) + 4.0f;
		const float spanY = (maxPos.y - minPos.y) + 4.0f;
		const float zForX = spanX * halfH / (viewW * 0.85f);
		const float zForY = spanY * halfH / (viewH * 0.85f);

		focusCameraOn((minPos + maxPos) * 0.5f, (std::max)(15.0f, (std::max)(zForX, zForY)));
	}

	bool collectBallPositionsAndCentroid(std::vector<vec2>& outPositions, vec2& outCenter) const
	{
		outPositions.clear();
		outCenter = vec2(0.0f);
		if (m_tempRegistry == nullptr || m_balls.empty())
		{
			return false;
		}

		outPositions.resize(m_balls.size(), vec2(0.0f));
		size_t count = 0;
		for (size_t i = 0; i < m_balls.size(); ++i)
		{
			const Entity ent = m_balls[i].entity;
			if (!hasTransform(ent))
			{
				continue;
			}
			const auto& t = m_tempRegistry->getComponent<Transform>(ent);
			outPositions[i] = vec2(t.position.x, t.position.y);
			outCenter += outPositions[i];
			++count;
		}
		if (count == 0)
		{
			outPositions.clear();
			return false;
		}
		outCenter /= static_cast<float>(count);
		return true;
	}

	void writeBallPositionsToECS(const std::vector<vec2>& positions)
	{
		if (m_tempRegistry == nullptr)
		{
			return;
		}
		const size_t count = (std::min)(m_balls.size(), positions.size());
		for (size_t i = 0; i < count; ++i)
		{
			const Entity ent = m_balls[i].entity;
			if (hasTransform(ent))
			{
				auto& t = m_tempRegistry->getComponent<Transform>(ent);
				t.position.x = positions[i].x;
				t.position.y = positions[i].y;
				m_tempRegistry->setComponentDirty(t);
			}
			if (m_tempRegistry->hasComponent<RigidBody2D>(ent))
			{
				auto& rb = m_tempRegistry->getComponent<RigidBody2D>(ent);
				rb.velocity = vec2(0.0f);
				m_tempRegistry->setComponentDirty(rb);
			}
		}
	}

	void applyRotationFromPositions(const std::vector<vec2>& sourcePositions, vec2 center, float angleDeg)
	{
		if (sourcePositions.size() != m_balls.size())
		{
			return;
		}
		const float rad = glm::radians(angleDeg);
		const float cosA = std::cos(rad);
		const float sinA = std::sin(rad);

		std::vector<vec2> rotated(sourcePositions.size());
		for (size_t i = 0; i < sourcePositions.size(); ++i)
		{
			const vec2 rel = sourcePositions[i] - center;
			rotated[i] = center + vec2(rel.x * cosA - rel.y * sinA, rel.x * sinA + rel.y * cosA);
		}
		writeBallPositionsToECS(rotated);
	}

	void rotateClusterByDegrees(float angleDeg)
	{
		if (m_balls.size() < 2 || std::abs(angleDeg) < 1e-6f)
		{
			return;
		}

		std::vector<vec2> sourcePositions;
		vec2 center(0.0f);
		if (!collectBallPositionsAndCentroid(sourcePositions, center))
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();
		pushUndoState();
		if (m_isRelaxing)
		{
			m_isRelaxing = false;
			m_relaxationVelocities.clear();
		}

		applyRotationFromPositions(sourcePositions, center, angleDeg);
	}

	void resetRotateDragState()
	{
		m_isRotatingDrag = false;
		m_hasRotateUndo = false;
		m_rotateDragAngleDeg = 0.0f;
		m_rotateStartPositions.clear();
	}

	void centerClusters(bool recordUndo = false)
	{
		if (m_balls.empty())
		{
			return;
		}

		// Don't shift if any ball is fixed
		for (const BallInfo& ball : m_balls)
		{
			if (m_tempRegistry->hasComponent<RigidBody2D>(ball.entity) &&
				m_tempRegistry->getComponent<RigidBody2D>(ball.entity).isFixed)
			{
				return;
			}
		}

		std::vector<vec2> positions;
		vec2 centroid(0.0f);
		if (!collectBallPositionsAndCentroid(positions, centroid))
		{
			return;
		}

		if (recordUndo)
		{
			pushUndoState();
		}

		for (vec2& p : positions)
		{
			p -= centroid;
		}
		writeBallPositionsToECS(positions);
	}

	void startRelaxation()
	{
		if (m_isSimulating)
		{
			setSimulation(false);
		}

		if (m_balls.empty())
		{
			return;
		}

		pushUndoState();
		centerClusters(false);
		m_isRelaxing = true;
		m_relaxationStable = false;
		m_relaxationStableFrames = 0;
		m_relaxationStepCount = 0;
		m_relaxationMaxDrift = 0.0f;
		m_relaxationVelocities.clear();
	}

	void centerAndRelaxClusters()
	{
		startRelaxation();
	}

	bool isBallFixed(Entity ent) const
	{
		return m_tempRegistry != nullptr && m_tempRegistry->hasComponent<RigidBody2D>(ent) &&
			   m_tempRegistry->getComponent<RigidBody2D>(ent).isFixed;
	}

	struct ConflictStats
	{
		size_t strainedLinks = 0;
		size_t overlappingPairs = 0;
		float maxError = 0.0f;
	};

	ConflictStats computeConstraintConflictStats() const
	{
		ConflictStats stats;
		for (const DistanceLink& link : m_links)
		{
			if (!hasTransform(link.a) || !hasTransform(link.b))
			{
				continue;
			}
			const float lenErr = std::abs(getLinkCurrentDistance(link) - link.restDistance);
			const float minDist = minAllowedLinkDistance(link.type);
			const float minErr = (link.restDistance < minDist - 0.01f) ? (minDist - link.restDistance) : 0.0f;
			const float err = (std::max)(lenErr, minErr);
			if (err > 0.01f)
			{
				++stats.strainedLinks;
			}
			stats.maxError = (std::max)(stats.maxError, err);
		}

		constexpr float OVERLAP_TOL_SQ = (BALL_DIAMETER - 0.01f) * (BALL_DIAMETER - 0.01f);
		for (size_t i = 0; i < m_balls.size(); ++i)
		{
			const Entity entA = m_balls[i].entity;
			if (!hasTransform(entA))
			{
				continue;
			}
			const bool fixedA = isBallFixed(entA);
			const auto& tA = m_tempRegistry->getComponent<Transform>(entA);
			const vec2 posA(tA.position.x, tA.position.y);

			for (size_t j = i + 1; j < m_balls.size(); ++j)
			{
				const Entity entB = m_balls[j].entity;
				if (!hasTransform(entB))
				{
					continue;
				}
				if (fixedA && isBallFixed(entB))
				{
					continue;
				}
				const auto& tB = m_tempRegistry->getComponent<Transform>(entB);
				const vec2 posB(tB.position.x, tB.position.y);
				if (glm::length2(posB - posA) < OVERLAP_TOL_SQ)
				{
					++stats.overlappingPairs;
				}
			}
		}
		return stats;
	}

	void applyAllCurrentDistances(bool recordUndo)
	{
		if (m_links.empty())
		{
			return;
		}
		if (recordUndo)
		{
			pushUndoState();
		}
		invalidatePlaySnapshotIfSimulating();
		if (m_isRelaxing)
		{
			m_isRelaxing = false;
			m_relaxationVelocities.clear();
		}
		for (DistanceLink& link : m_links)
		{
			resizeLinkToCurrentDistance(link);
		}
		m_syncedLinkDist = -1.0f;
	}

	void relaxConstraintDistances()
	{
		solveDistanceLayout(false);
	}

	void minimizeConstraintDistances()
	{
		solveDistanceLayout(true);
	}

	void solveDistanceLayout(bool minimizeMode)
	{
		if (m_tempRegistry == nullptr || m_balls.size() < 2)
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();
		pushUndoState();

		if (m_isRelaxing)
		{
			m_isRelaxing = false;
			m_relaxationVelocities.clear();
		}

		const size_t n = m_balls.size();
		std::vector<vec2> positions(n, vec2(0.0f));
		std::vector<bool> isFixed(n, false);
		std::unordered_map<Entity, size_t> entityToIndex;
		entityToIndex.reserve(n);

		vec2 initialFreeCentroid(0.0f);
		size_t freeCount = 0;
		for (size_t i = 0; i < n; ++i)
		{
			const Entity ent = m_balls[i].entity;
			entityToIndex[ent] = i;
			if (hasTransform(ent))
			{
				const auto& t = m_tempRegistry->getComponent<Transform>(ent);
				positions[i] = vec2(t.position.x, t.position.y);
			}
			if (isBallFixed(ent))
			{
				isFixed[i] = true;
			}
			else
			{
				initialFreeCentroid += positions[i];
				++freeCount;
			}
		}

		struct SolverLink
		{
			size_t ia = 0;
			size_t ib = 0;
			float restDistance = 1.0f;
			float weight = 1.0f;
		};

		std::vector<SolverLink> solverLinks;
		solverLinks.reserve(m_links.size());
		std::vector<bool> hasAnyLink(n, false);

		for (const DistanceLink& link : m_links)
		{
			auto itA = entityToIndex.find(link.a);
			auto itB = entityToIndex.find(link.b);
			if (itA == entityToIndex.end() || itB == entityToIndex.end() || itA->second == itB->second)
			{
				continue;
			}
			const size_t ia = itA->second;
			const size_t ib = itB->second;
			const float weight = (link.type == LinkType::Distance) ? 1.0f : (std::max)(0.01f, getLinkStiffness(link));
			// Rigidbodies cannot rest closer than touching diameter 1.0 without colliding.
			solverLinks.push_back({ia, ib, (std::max)(link.restDistance, BALL_DIAMETER), weight});
			hasAnyLink[ia] = true;
			hasAnyLink[ib] = true;
		}

		auto resolveNonOverlapContacts = [&](int passes)
		{
			for (int p = 0; p < passes; ++p)
			{
				for (size_t i = 0; i < n; ++i)
				{
					for (size_t j = i + 1; j < n; ++j)
					{
						if (isFixed[i] && isFixed[j])
						{
							continue;
						}
						const vec2 diff = positions[j] - positions[i];
						const float distSq = glm::length2(diff);
						if (distSq >= BALL_DIAMETER * BALL_DIAMETER)
						{
							continue;
						}
						float dist = std::sqrt(distSq);
						vec2 dir;
						if (dist < 1e-4f)
						{
							const float angle = static_cast<float>(i * 2.3999632f + j);
							dir = vec2(std::cos(angle), std::sin(angle));
							dist = 1e-4f;
						}
						else
						{
							dir = diff / dist;
						}
						const float overlap = BALL_DIAMETER - dist;
						if (isFixed[i])
						{
							positions[j] += dir * overlap;
						}
						else if (isFixed[j])
						{
							positions[i] -= dir * overlap;
						}
						else
						{
							positions[i] -= dir * (0.5f * overlap);
							positions[j] += dir * (0.5f * overlap);
						}
					}
				}
			}
		};

		if (freeCount > 0)
		{
			initialFreeCentroid /= static_cast<float>(freeCount);

			std::vector<vec2> prevPositions(n);
			std::vector<vec2> sumDisps(n, vec2(0.0f));
			std::vector<float> sumWeights(n, 0.0f);

			const int maxIters = minimizeMode ? 400 : 300;
			constexpr float STEP_DAMPING = 0.5f;
			constexpr float CONVERGE_EPS = 1e-4f;

			for (int iter = 0; iter < maxIters; ++iter)
			{
				prevPositions = positions;
				std::fill(sumDisps.begin(), sumDisps.end(), vec2(0.0f));
				std::fill(sumWeights.begin(), sumWeights.end(), 0.0f);

				auto accumulatePair =
					[&](size_t ia, size_t ib, vec2 diff, float targetDist, float weight, float fallbackSeed)
				{
					float dist = glm::length(diff);
					vec2 dir;
					if (dist < 1e-4f)
					{
						dir = vec2(std::cos(fallbackSeed), std::sin(fallbackSeed));
						dist = 1e-4f;
					}
					else
					{
						dir = diff / dist;
					}

					const float err = dist - targetDist;
					const float wA = isFixed[ia] ? 0.0f : 1.0f;
					const float wB = isFixed[ib] ? 0.0f : 1.0f;
					const vec2 corr = dir * (err / (wA + wB)) * weight;
					if (!isFixed[ia])
					{
						sumDisps[ia] += corr * wA;
						sumWeights[ia] += weight;
					}
					if (!isFixed[ib])
					{
						sumDisps[ib] -= corr * wB;
						sumWeights[ib] += weight;
					}
				};

				for (size_t li = 0; li < solverLinks.size(); ++li)
				{
					const SolverLink& sl = solverLinks[li];
					if (isFixed[sl.ia] && isFixed[sl.ib])
					{
						continue;
					}
					const vec2 diff = positions[sl.ib] - positions[sl.ia];
					if (minimizeMode)
					{
						if (glm::length2(diff) > BALL_DIAMETER * BALL_DIAMETER)
						{
							accumulatePair(sl.ia, sl.ib, diff, BALL_DIAMETER, 1.0f,
										   static_cast<float>(li) * 2.3999632f);
						}
					}
					else
					{
						accumulatePair(sl.ia, sl.ib, diff, sl.restDistance, sl.weight,
									   static_cast<float>(li) * 2.3999632f);
					}
				}

				if (minimizeMode)
				{
					for (size_t i = 0; i < n; ++i)
					{
						if (!isFixed[i] && !hasAnyLink[i])
						{
							constexpr float LOOSE_WEIGHT = 0.25f;
							sumDisps[i] += (initialFreeCentroid - positions[i]) * LOOSE_WEIGHT;
							sumWeights[i] += LOOSE_WEIGHT;
						}
					}
				}

				for (size_t i = 0; i < n; ++i)
				{
					if (isFixed[i] || sumWeights[i] <= 0.0f)
					{
						continue;
					}
					positions[i] += (sumDisps[i] / sumWeights[i]) * STEP_DAMPING;
				}

				// Separate all overlapping bodies (including linked pairs) back to touching diameter 1.0.
				resolveNonOverlapContacts(minimizeMode ? 4 : 2);

				float maxMove = 0.0f;
				for (size_t i = 0; i < n; ++i)
				{
					maxMove = (std::max)(maxMove, glm::length(positions[i] - prevPositions[i]));
				}
				if (maxMove < CONVERGE_EPS)
				{
					break;
				}
			}

			resolveNonOverlapContacts(8);

			if (freeCount == n)
			{
				vec2 finalCentroid(0.0f);
				for (size_t i = 0; i < n; ++i)
				{
					finalCentroid += positions[i];
				}
				finalCentroid /= static_cast<float>(n);
				const vec2 shift = initialFreeCentroid - finalCentroid;
				for (size_t i = 0; i < n; ++i)
				{
					positions[i] += shift;
				}
			}
		}

		writeBallPositionsToECS(positions);
		applyAllCurrentDistances(false);
		m_relaxationStable = true;
		WeirdEngine::Logger::log(minimizeMode
									 ? "[ClusterEditor] Minimized cluster distances cleanly without overlap."
									 : "[ClusterEditor] Relaxed constraint distances to feasible equilibrium.");
	}

	void stepRelaxation(float deltaTime)
	{
		if (m_balls.empty())
		{
			m_isRelaxing = false;
			return;
		}

		const size_t n = m_balls.size();
		std::vector<vec2> positions(n);
		std::vector<bool> isFixed(n, false);
		std::unordered_map<Entity, size_t> entityToIndex;

		for (size_t i = 0; i < n; ++i)
		{
			entityToIndex[m_balls[i].entity] = i;
			if (hasTransform(m_balls[i].entity))
			{
				const auto& t = m_tempRegistry->getComponent<Transform>(m_balls[i].entity);
				positions[i] = vec2(t.position.x, t.position.y);
			}
			if (m_tempRegistry->hasComponent<RigidBody2D>(m_balls[i].entity))
			{
				isFixed[i] = m_tempRegistry->getComponent<RigidBody2D>(m_balls[i].entity).isFixed;
			}
		}

		std::vector<vec2> forces(n, vec2(0.0f));

		// 1. Pairwise repulsion between rigid bodies
		for (size_t i = 0; i < n; ++i)
		{
			for (size_t j = i + 1; j < n; ++j)
			{
				const bool linked = linkExists(m_balls[i].entity, m_balls[j].entity);
				vec2 diff = positions[i] - positions[j];
				float dist = glm::length(diff);
				vec2 dir;
				if (dist < 0.0001f)
				{
					float angle = static_cast<float>(i * 2.3999632f + j);
					dir = vec2(std::cos(angle), std::sin(angle));
					dist = 0.0001f;
				}
				else
				{
					dir = diff / dist;
				}

				if (!linked)
				{
					float repForce = 0.0f;
					// Overlap spring force if balls penetrate
					if (dist < 1.0f)
					{
						repForce += 15.0f * (1.0f - dist);
					}
					// Non-bonded Coulomb-like repulsion
					constexpr float maxRepDist = 15.0f;
					if (dist < maxRepDist)
					{
						repForce += m_repulsionStrength / (dist * dist + 0.1f);
					}

					if (!isFixed[i])
					{
						forces[i] += dir * repForce;
					}
					if (!isFixed[j])
					{
						forces[j] -= dir * repForce;
					}
				}
				else
				{
					// For linked balls, prevent severe core penetration
					if (dist < 0.8f)
					{
						float overlapForce = 15.0f * (0.8f - dist);
						if (!isFixed[i])
						{
							forces[i] += dir * overlapForce;
						}
						if (!isFixed[j])
						{
							forces[j] -= dir * overlapForce;
						}
					}
				}
			}
		}

		// 2. Link restorative forces
		for (const auto& link : m_links)
		{
			auto itA = entityToIndex.find(link.a);
			auto itB = entityToIndex.find(link.b);
			if (itA == entityToIndex.end() || itB == entityToIndex.end())
			{
				continue;
			}
			size_t ia = itA->second;
			size_t ib = itB->second;

			vec2 diff = positions[ia] - positions[ib];
			float dist = glm::length(diff);
			vec2 dir;
			if (dist < 0.0001f)
			{
				dir = vec2(1.0f, 0.0f);
				dist = 0.0001f;
			}
			else
			{
				dir = diff / dist;
			}

			float deltaDist = dist - link.restDistance;

			if (link.type == LinkType::Spring)
			{
				float springK = SPRING_STIFFNESS * 20.0f;
				float f = -springK * deltaDist;
				if (!isFixed[ia])
				{
					forces[ia] += dir * f;
				}
				if (!isFixed[ib])
				{
					forces[ib] -= dir * f;
				}
			}
			else
			{
				float distK = CONSTRAINT_STIFFNESS * 40.0f;
				float f = -distK * deltaDist;
				if (!isFixed[ia])
				{
					forces[ia] += dir * f;
				}
				if (!isFixed[ib])
				{
					forces[ib] -= dir * f;
				}
			}
		}

		// 3. Overdamped integration
		const float dt = (std::clamp)(deltaTime, 0.001f, 0.033f) * m_relaxationSpeed;
		constexpr float maxStep = 0.08f;
		float maxDrift = 0.0f;

		for (size_t i = 0; i < n; ++i)
		{
			if (isFixed[i])
			{
				continue;
			}

			Entity ent = m_balls[i].entity;
			vec2 vel = m_relaxationVelocities[ent];
			vel += forces[i] * dt;
			vel *= (std::max)(0.0f, 1.0f - 8.0f * dt);

			vec2 disp = vel * dt;
			float stepLen = glm::length(disp);
			if (stepLen > maxStep)
			{
				disp = (disp / stepLen) * maxStep;
				vel = disp / dt;
			}

			positions[i] += disp;
			m_relaxationVelocities[ent] = vel;
			maxDrift = (std::max)(maxDrift, stepLen);
		}

		// 4. DistanceConstraint position-based correction pass
		for (const auto& link : m_links)
		{
			if (link.type != LinkType::Distance)
			{
				continue;
			}
			auto itA = entityToIndex.find(link.a);
			auto itB = entityToIndex.find(link.b);
			if (itA == entityToIndex.end() || itB == entityToIndex.end())
			{
				continue;
			}
			size_t ia = itA->second;
			size_t ib = itB->second;

			vec2 diff = positions[ia] - positions[ib];
			float dist = glm::length(diff);
			if (dist < 0.0001f)
			{
				continue;
			}
			vec2 dir = diff / dist;
			float error = dist - link.restDistance;
			constexpr float correctionRate = 0.5f;

			if (!isFixed[ia] && !isFixed[ib])
			{
				positions[ia] -= dir * (0.5f * error * correctionRate);
				positions[ib] += dir * (0.5f * error * correctionRate);
			}
			else if (!isFixed[ia])
			{
				positions[ia] -= dir * (error * correctionRate);
			}
			else if (!isFixed[ib])
			{
				positions[ib] += dir * (error * correctionRate);
			}
		}

		// 5. Keep centroid at origin if no fixed balls
		bool anyFixed = false;
		for (size_t i = 0; i < n; ++i)
		{
			if (isFixed[i])
			{
				anyFixed = true;
				break;
			}
		}

		if (!anyFixed)
		{
			vec2 centroid(0.0f);
			for (size_t i = 0; i < n; ++i)
			{
				centroid += positions[i];
			}
			centroid /= static_cast<float>(n);

			for (size_t i = 0; i < n; ++i)
			{
				positions[i] -= centroid;
			}
		}

		// 6. Write back positions to ECS
		for (size_t i = 0; i < n; ++i)
		{
			Entity ent = m_balls[i].entity;
			if (hasTransform(ent))
			{
				auto& t = m_tempRegistry->getComponent<Transform>(ent);
				t.position.x = positions[i].x;
				t.position.y = positions[i].y;
				m_tempRegistry->setComponentDirty(t);
			}
			if (m_tempRegistry->hasComponent<RigidBody2D>(ent))
			{
				auto& rb = m_tempRegistry->getComponent<RigidBody2D>(ent);
				rb.velocity = vec2(0.0f);
				m_tempRegistry->setComponentDirty(rb);
			}
		}

		// 7. Convergence check
		m_relaxationMaxDrift = maxDrift;
		m_relaxationStepCount++;

		constexpr float STABILITY_DRIFT_THRESHOLD = 0.0015f;
		if (maxDrift < STABILITY_DRIFT_THRESHOLD)
		{
			m_relaxationStableFrames++;
			if (m_relaxationStableFrames >= 25)
			{
				m_isRelaxing = false;
				m_relaxationStable = true;
				m_relaxationVelocities.clear();
				WeirdEngine::Logger::log("[ClusterEditor] Cluster reached stable relaxed position in " +
										 std::to_string(m_relaxationStepCount) + " steps.");
			}
		}
		else
		{
			m_relaxationStableFrames = 0;
			m_relaxationStable = false;
		}

		if (m_relaxationStepCount >= 600)
		{
			m_isRelaxing = false;
			m_relaxationStable = true;
			m_relaxationVelocities.clear();
			WeirdEngine::Logger::log("[ClusterEditor] Cluster relaxation finished (max steps reached).");
		}
	}

	void fitClustersToGrid()
	{
		if (!gridEnabled() || m_balls.empty())
		{
			return;
		}

		pushUndoState();

		for (const BallInfo& ball : m_balls)
		{
			snapBallToGrid(ball);
		}

		// Snap positions changed the geometry: resize every link to match it.
		applyAllCurrentDistances(false);
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

	ClusterSnapshotState captureClusterState() const
	{
		ClusterSnapshotState s;
		s.balls = m_balls;
		s.links = m_links;
		s.tagSelectedEntity = m_tagSelectedEntity;
		if (m_tempSvc)
		{
			for (const auto& ball : m_balls)
			{
				if (m_tempSvc->names().hasName(ball.entity))
				{
					s.names[ball.entity] = m_tempSvc->names().getEntityName(ball.entity);
				}
				const auto& tags = m_tempSvc->tags().getTags(ball.entity);
				if (!tags.empty())
				{
					s.tags[ball.entity] = tags;
				}
			}
		}
		return s;
	}

	void applyClusterState(const ClusterSnapshotState& s)
	{
		m_balls = s.balls;
		m_links = s.links;
		m_tagSelectedEntity = s.tagSelectedEntity;
		if (m_tempSvc)
		{
			for (const auto& [entity, name] : s.names)
			{
				m_tempSvc->names().setName(entity, name);
			}
			for (const auto& [entity, tags] : s.tags)
			{
				m_tempSvc->tags().removeAllTags(entity);
				for (const auto& tag : tags)
				{
					m_tempSvc->tags().addTag(entity, tag);
				}
			}
		}

		m_draggedBall = static_cast<Entity>(-1);
		m_draggedSimulationId = INVALID_SIMULATION_ID;
		m_constraintStartBall = static_cast<Entity>(-1);
		m_draggedLinkConstraint = static_cast<Entity>(-1);
		m_modifyPressPending = false;
		m_modifyDragActive = false;
		m_modifyPressedLink = static_cast<Entity>(-1);
		pruneLinkSelection();
	}

	void invalidatePlaySnapshotIfSimulating()
	{
		if (m_isSimulating)
		{
			m_hasPlaySnapshot = false;
		}
	}

	void toggleSimulation()
	{
		setSimulation(!m_isSimulating);
	}

	void setSimulation(bool enable)
	{
		if (m_isSimulating == enable)
		{
			return;
		}

		m_isSimulating = enable;

		if (m_tempSvc == nullptr)
		{
			return;
		}

		if (m_isSimulating)
		{
			if (m_isRelaxing)
			{
				m_isRelaxing = false;
				m_relaxationVelocities.clear();
			}

			// Take temporary snapshot before physics simulation
			m_playSnapshot = captureClusterState();
			m_tempSvc->serialization().saveSnapshot("temp_cluster_editor.weirdsnap");
			m_hasPlaySnapshot = true;

			m_tempSvc->physics().setGravity(m_gravityEnabled ? m_playGravity : 0.0f);
			m_tempSvc->physics().setDamping(m_playDamping);
			Logger::log("[ClusterEditor] Simulation started (play mode).");
		}
		else
		{
			m_tempSvc->physics().setGravity(0.0f);
			m_tempSvc->physics().setDamping(1.0f);

			// Revert to snapshot on pause when no structural edits invalidated it
			if (m_hasPlaySnapshot)
			{
				if (m_tempSvc->serialization().loadSnapshot("temp_cluster_editor.weirdsnap"))
				{
					applyClusterState(m_playSnapshot);
					Logger::log("[ClusterEditor] Simulation paused (design mode, reverted to snapshot).");
				}
			}
			else
			{
				Logger::log("[ClusterEditor] Simulation paused (edits during play kept).");
			}
		}
	}

	void takeManualSnapshot()
	{
		if (m_tempSvc == nullptr)
		{
			return;
		}
		m_manualSnapshot = captureClusterState();
		m_tempSvc->serialization().saveSnapshot("dev.weirdsnap");
		m_hasManualSnapshot = true;
	}

	void restoreManualSnapshot()
	{
		if (m_tempSvc == nullptr || !m_hasManualSnapshot)
		{
			return;
		}
		pushUndoState();
		if (m_tempSvc->serialization().loadSnapshot("dev.weirdsnap"))
		{
			applyClusterState(m_manualSnapshot);
		}
	}

	bool canUndo() const
	{
		return !m_undoStack.empty();
	}

	bool canRedo() const
	{
		return !m_redoStack.empty();
	}

	void clearUndoHistory()
	{
		m_undoStack.clear();
		m_redoStack.clear();
		m_hasPendingBallDragUndo = false;
		m_resizedLinksDuringBallDrag = false;
		m_hasPendingLinkDragUndo = false;
		m_hasUiDragUndo = false;
		resetRotateDragState();
	}

	ClusterUndoState captureUndoState() const
	{
		ClusterUndoState state;
		if (m_tempRegistry == nullptr)
		{
			return state;
		}

		std::unordered_map<Entity, size_t> entityToIndex;
		entityToIndex.reserve(m_balls.size());

		for (size_t i = 0; i < m_balls.size(); ++i)
		{
			const Entity e = m_balls[i].entity;
			if (e == m_tagSelectedEntity)
			{
				state.selectedBallIndex = static_cast<int>(state.balls.size());
			}

			UndoBallState bs;
			if (m_tempRegistry->getComponentArray<Transform>()->hasData(e))
			{
				const auto& t = m_tempRegistry->getComponent<Transform>(e);
				bs.position = vec2(t.position.x, t.position.y);
			}
			if (m_tempRegistry->hasComponent<Dot>(e))
			{
				bs.materialId = m_tempRegistry->getComponent<Dot>(e).materialId;
			}
			if (m_tempRegistry->hasComponent<RigidBody2D>(e))
			{
				bs.isFixed = m_tempRegistry->getComponent<RigidBody2D>(e).isFixed;
			}
			if (m_tempSvc != nullptr)
			{
				if (m_tempSvc->names().hasName(e))
				{
					bs.name = m_tempSvc->names().getEntityName(e);
				}
				bs.tags = m_tempSvc->tags().getTags(e);
			}

			entityToIndex[e] = state.balls.size();
			state.balls.push_back(std::move(bs));
		}

		state.links.reserve(m_links.size());
		for (const DistanceLink& link : m_links)
		{
			auto itA = entityToIndex.find(link.a);
			auto itB = entityToIndex.find(link.b);
			if (itA == entityToIndex.end() || itB == entityToIndex.end())
			{
				continue;
			}

			UndoLinkState ls;
			ls.indexA = itA->second;
			ls.indexB = itB->second;
			ls.type = link.type;
			ls.restDistance = link.restDistance;
			if (link.type == LinkType::Spring && m_tempRegistry->hasComponent<Spring>(link.constraintEntity))
			{
				ls.stiffness = m_tempRegistry->getComponent<Spring>(link.constraintEntity).stiffness;
			}
			if (isLinkSelected(link.constraintEntity))
			{
				state.selectedLinkIndices.push_back(state.links.size());
			}
			state.links.push_back(ls);
		}

		return state;
	}

	void commitUndoState(const ClusterUndoState& snapshot)
	{
		m_undoStack.push_back(snapshot);
		if (m_undoStack.size() > MAX_UNDO_HISTORY)
		{
			m_undoStack.erase(m_undoStack.begin());
		}
		m_redoStack.clear();
	}

	void pushUndoState()
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}
		commitUndoState(captureUndoState());
	}

	void undo()
	{
		if (!canUndo() || m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}

		m_redoStack.push_back(captureUndoState());
		if (m_redoStack.size() > MAX_UNDO_HISTORY)
		{
			m_redoStack.erase(m_redoStack.begin());
		}

		ClusterUndoState target = std::move(m_undoStack.back());
		m_undoStack.pop_back();
		applyUndoState(target);
	}

	void redo()
	{
		if (!canRedo() || m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}

		m_undoStack.push_back(captureUndoState());
		if (m_undoStack.size() > MAX_UNDO_HISTORY)
		{
			m_undoStack.erase(m_undoStack.begin());
		}

		ClusterUndoState target = std::move(m_redoStack.back());
		m_redoStack.pop_back();
		applyUndoState(target);
	}

	Entity createBallEntity(vec2 pos, unsigned int materialId, bool isFixed)
	{
		Entity e = m_tempRegistry->createEntity();
		if (e == INVALID_ENTITY)
		{
			return INVALID_ENTITY;
		}

		auto& t = m_tempRegistry->addComponent<Transform>(e);
		t.position = vec3(pos.x, pos.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		auto& dot = m_tempRegistry->addComponent<Dot>(e);
		dot.materialId = materialId;

		auto& rb = m_tempRegistry->addComponent<RigidBody2D>(e);
		rb.isFixed = isFixed;
		if (isFixed)
		{
			m_tempRegistry->setComponentDirty(rb);
		}

		m_balls.push_back({e, rb.simulationId});
		return e;
	}

	Entity createConstraintComponentEntity(Entity a, Entity b, LinkType type, float restDistance, float stiffness)
	{
		Entity constraintEnt = m_tempRegistry->createEntity();
		if (constraintEnt == INVALID_ENTITY)
		{
			return INVALID_ENTITY;
		}

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
			spring.stiffness = (std::clamp)(stiffness, 0.001f, 1.0f);
			spring.restDistance = restDistance;
		}
		return constraintEnt;
	}

	void destroyClusterEntities()
	{
		for (DistanceLink& link : m_links)
		{
			m_tempRegistry->destroyEntity(link.constraintEntity);
		}
		m_links.clear();
		m_tempRegistry->freeRemovedComponents();

		for (const BallInfo& ball : m_balls)
		{
			m_tempSvc->names().removeName(ball.entity);
			m_tempSvc->tags().removeAllTags(ball.entity);
			m_tempRegistry->destroyEntity(ball.entity);
		}
		m_balls.clear();
		m_tempRegistry->freeRemovedComponents();
	}

	void applyUndoState(const ClusterUndoState& state)
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();

		m_isRelaxing = false;
		m_relaxationStable = false;
		m_relaxationVelocities.clear();

		m_dragMode = DragMode::None;
		m_draggedBall = static_cast<Entity>(-1);
		m_draggedSimulationId = INVALID_SIMULATION_ID;
		m_keepFixedAfterDrag = false;
		m_constraintStartBall = static_cast<Entity>(-1);
		m_draggedLinkConstraint = static_cast<Entity>(-1);
		m_uiHoveredLink = static_cast<Entity>(-1);
		m_modifyPressPending = false;
		m_modifyDragActive = false;
		m_modifyPressedLink = static_cast<Entity>(-1);
		m_hasPendingBallDragUndo = false;
		m_resizedLinksDuringBallDrag = false;
		m_hasPendingLinkDragUndo = false;
		m_hasUiDragUndo = false;
		resetRotateDragState();
		clearTagSelection();
		clearLinkSelection();

		destroyClusterEntities();

		std::vector<Entity> createdBalls;
		createdBalls.reserve(state.balls.size());

		for (const UndoBallState& bs : state.balls)
		{
			Entity e = createBallEntity(bs.position, bs.materialId, bs.isFixed);
			createdBalls.push_back(e);
			if (e == INVALID_ENTITY)
			{
				continue;
			}

			if (!bs.name.empty())
			{
				m_tempSvc->names().setName(e, bs.name);
			}
			for (const std::string& tag : bs.tags)
			{
				m_tempSvc->tags().addTag(e, tag);
			}
		}

		std::unordered_set<size_t> selectedLinkSet(state.selectedLinkIndices.begin(), state.selectedLinkIndices.end());
		for (size_t i = 0; i < state.links.size(); ++i)
		{
			const UndoLinkState& ls = state.links[i];
			if (ls.indexA >= createdBalls.size() || ls.indexB >= createdBalls.size())
			{
				continue;
			}
			const Entity a = createdBalls[ls.indexA];
			const Entity b = createdBalls[ls.indexB];
			if (!isValidEntityId(a) || !isValidEntityId(b) || a == b)
			{
				continue;
			}

			Entity constraintEnt = createConstraintComponentEntity(a, b, ls.type, ls.restDistance, ls.stiffness);
			if (constraintEnt == INVALID_ENTITY)
			{
				continue;
			}

			const SimulationID idA = getSimulationId(a);
			const SimulationID idB = getSimulationId(b);
			m_links.push_back({a, b, idA, idB, ls.restDistance, ls.type, constraintEnt});
			if (selectedLinkSet.count(i) > 0)
			{
				m_selectedLinkConstraints.push_back(constraintEnt);
			}
		}

		if (m_toolMode == ToolMode::TagEditor && state.selectedBallIndex >= 0 &&
			static_cast<size_t>(state.selectedBallIndex) < createdBalls.size())
		{
			const Entity restoredSel = createdBalls[static_cast<size_t>(state.selectedBallIndex)];
			if (isValidEntityId(restoredSel))
			{
				m_tagSelectedEntity = restoredSel;
				m_tagBufferEntity = static_cast<Entity>(-1);
			}
		}

		updateTagEditor();
	}

	void applyGravitySettings()
	{
		if (m_isSimulating && m_tempSvc != nullptr)
		{
			m_tempSvc->physics().setGravity(m_gravityEnabled ? m_playGravity : 0.0f);
		}
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

	bool resizeLinkToCurrentDistance(DistanceLink& link)
	{
		if (!hasTransform(link.a) || !hasTransform(link.b))
		{
			return false;
		}

		const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
		const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
		const vec2 pa(ta.position.x, ta.position.y);
		const vec2 pb(tb.position.x, tb.position.y);

		// Auto-resize has no upper bound: the link follows wherever the atom was dragged or snapped.
		const float nextDistance = (std::max)(glm::length(pb - pa), minAllowedLinkDistance(link.type));
		if (std::abs(link.restDistance - nextDistance) <= 1e-5f)
		{
			return false;
		}

		link.restDistance = nextDistance;
		applyLinkDistance(link);
		return true;
	}

	void resizeLinksForBall(Entity ball)
	{
		bool anyResized = false;
		for (DistanceLink& link : m_links)
		{
			if (link.a == ball || link.b == ball)
			{
				anyResized |= resizeLinkToCurrentDistance(link);
			}
		}
		if (anyResized)
		{
			m_syncedLinkDist = -1.0f;
			invalidatePlaySnapshotIfSimulating();
			if (ball == m_draggedBall && m_dragMode == DragMode::Ball)
			{
				m_resizedLinksDuringBallDrag = true;
			}
		}
	}

	void removeLinkAt(size_t index)
	{
		if (index >= m_links.size() || m_tempRegistry == nullptr)
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();

		DistanceLink& link = m_links[index];
		if (m_draggedLinkConstraint == link.constraintEntity)
		{
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			m_hasPendingLinkDragUndo = false;
		}
		if (m_modifyPressedLink == link.constraintEntity)
		{
			m_modifyPressedLink = static_cast<Entity>(-1);
			m_modifyPressPending = false;
			m_modifyDragActive = false;
		}
		if (m_uiHoveredLink == link.constraintEntity)
		{
			m_uiHoveredLink = static_cast<Entity>(-1);
		}
		m_selectedLinkConstraints.erase(
			std::remove(m_selectedLinkConstraints.begin(), m_selectedLinkConstraints.end(), link.constraintEntity),
			m_selectedLinkConstraints.end());

		Entity constraintEnt = link.constraintEntity;

		m_tempRegistry->destroyEntity(constraintEnt);
		m_links.erase(m_links.begin() + static_cast<std::ptrdiff_t>(index));
	}

	void removeBall(Entity e)
	{
		if (!isValidEntityId(e) || m_tempRegistry == nullptr)
		{
			return;
		}

		invalidatePlaySnapshotIfSimulating();

		// 1. Remove all links in m_links connected to this ball
		for (size_t i = 0; i < m_links.size();)
		{
			if (m_links[i].a == e || m_links[i].b == e)
			{
				removeLinkAt(i);
			}
			else
			{
				++i;
			}
		}

		// 2. Sweep registry for any remaining distance constraints or springs referencing this entity
		auto dcArray = m_tempRegistry->getComponentArray<DistanceConstraint>();
		if (dcArray)
		{
			for (size_t i = 0; i < dcArray->getSize(); ++i)
			{
				auto& dc = dcArray->getDataAtIdx(i);
				if (dc.entityA == e || dc.entityB == e)
				{
					Entity dcEnt = dcArray->getEntityAtIdx(i);
					m_tempRegistry->destroyEntity(dcEnt);
				}
			}
		}

		auto springArray = m_tempRegistry->getComponentArray<Spring>();
		if (springArray)
		{
			for (size_t i = 0; i < springArray->getSize(); ++i)
			{
				auto& sp = springArray->getDataAtIdx(i);
				if (sp.entityA == e || sp.entityB == e)
				{
					Entity spEnt = springArray->getEntityAtIdx(i);
					m_tempRegistry->destroyEntity(spEnt);
				}
			}
		}

		// Process constraint removals in ECS and physics simulation while the ball still exists
		m_tempRegistry->freeRemovedComponents();

		// 3. Remove name and tags
		if (m_tempSvc)
		{
			m_tempSvc->names().removeName(e);
			m_tempSvc->tags().removeAllTags(e);
		}

		// 4. Remove from m_balls
		m_balls.erase(std::remove_if(m_balls.begin(), m_balls.end(), [e](const BallInfo& b) { return b.entity == e; }),
					  m_balls.end());

		// 5. Clean up state references
		if (m_draggedBall == e)
		{
			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = INVALID_SIMULATION_ID;
			m_keepFixedAfterDrag = false;
			m_hasPendingBallDragUndo = false;
			m_resizedLinksDuringBallDrag = false;
			m_dragMode = DragMode::None;
		}
		if (m_constraintStartBall == e)
		{
			m_constraintStartBall = static_cast<Entity>(-1);
		}
		if (m_tagSelectedEntity == e)
		{
			clearTagSelection();
		}
		m_relaxationVelocities.erase(e);

		// 6. Destroy ball entity in registry and flush deferred ECS removals
		m_tempRegistry->destroyEntity(e);
		m_tempRegistry->freeRemovedComponents();

		// 7. Refresh simulation IDs in m_balls and m_links (due to particle swap in Simulation2D)
		refreshSimulationIds();
	}

	// -----------------------------------------------------------------------
	// Scene files
	// -----------------------------------------------------------------------

	void clearCluster()
	{
		if (m_isSimulating)
		{
			m_hasPlaySnapshot = false;
			setSimulation(false);
		}
		m_hasPlaySnapshot = false;
		m_hasManualSnapshot = false;

		destroyClusterEntities();

		m_draggedBall = static_cast<Entity>(-1);
		m_draggedSimulationId = INVALID_SIMULATION_ID;
		m_draggedLinkConstraint = static_cast<Entity>(-1);
		m_constraintStartBall = static_cast<Entity>(-1);
		m_uiHoveredLink = static_cast<Entity>(-1);
		m_modifyPressPending = false;
		m_modifyDragActive = false;
		m_modifyPressedLink = static_cast<Entity>(-1);
		m_hasPendingBallDragUndo = false;
		m_resizedLinksDuringBallDrag = false;
		m_hasPendingLinkDragUndo = false;
		m_hasUiDragUndo = false;
		resetRotateDragState();
		m_keepFixedAfterDrag = false;
		m_dragMode = DragMode::None;
		m_rightWasDown = false;
		clearTagSelection();
		clearLinkSelection();
		m_currentFilePath.clear();
		std::snprintf(m_fileNameBuf, sizeof(m_fileNameBuf), "cluster");
		m_isRelaxing = false;
		m_relaxationStable = false;
		m_relaxationVelocities.clear();
	}

	void saveCluster()
	{
		if (m_isSimulating)
		{
			setSimulation(false);
		}

		if (m_currentFilePath.empty())
		{
			openSaveFileDialog();
			return;
		}
		saveClusterTo(m_currentFilePath);
	}

	void saveClusterTo(const std::string& path)
	{
		if (m_isSimulating)
		{
			setSimulation(false);
		}

		std::string finalPath = path;
		if (!finalPath.ends_with(".weirdcluster"))
		{
			finalPath += ".weirdcluster";
		}

		m_tempSvc->serialization().saveScene(finalPath);
		m_currentFilePath = finalPath;
		WeirdEngine::Logger::log("Saved cluster to " + finalPath);
	}

	void openLoadFileDialog()
	{
#ifdef __EMSCRIPTEN__
		loadClusterFromName();
#else
		static SDL_DialogFileFilter filters[1] = {{"Weird Cluster (*.weirdcluster)", "weirdcluster"}};
		SDL_ShowOpenFileDialog(onOpenFileCallback, this, nullptr, filters, 1, nullptr, false);
#endif
	}

	void openSaveFileDialog()
	{
#ifdef __EMSCRIPTEN__
		saveClusterFromName();
#else
		static SDL_DialogFileFilter filters[1] = {{"Weird Cluster (*.weirdcluster)", "weirdcluster"}};
		SDL_ShowSaveFileDialog(onSaveFileCallback, this, nullptr, filters, 1, "cluster.weirdcluster");
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
			loadCluster(loadPath);
			m_currentFilePath = loadPath;
		}

		if (!savePath.empty())
		{
			saveClusterTo(savePath);
		}
	}

#ifndef __EMSCRIPTEN__
	static void SDLCALL onOpenFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* editor = static_cast<ClusterEditor*>(userdata);
		if (!editor || !filelist || !*filelist)
		{
			return;
		}

		std::lock_guard<std::mutex> lock(editor->m_fileMutex);
		editor->m_pendingLoadPath = *filelist;
	}

	static void SDLCALL onSaveFileCallback(void* userdata, const char* const* filelist, int filter)
	{
		auto* editor = static_cast<ClusterEditor*>(userdata);
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
			fileName = "cluster";
		}
		if (!fileName.ends_with(".weirdcluster"))
		{
			fileName += ".weirdcluster";
		}
		return fileName;
	}

	void saveClusterFromName()
	{
		saveClusterTo(m_tempSvc->resources().assetPath("Organisms/") + currentFileName());
	}

	void loadClusterFromName()
	{
		// Only used by the Emscripten file fallback; keep it referenced everywhere
		// to avoid dead-code surprises when building for the web.
		const std::string path = m_tempSvc->resources().assetPath("Organisms/") + currentFileName();
		loadCluster(path);
		m_currentFilePath = path;
	}

	void removeFallenBalls()
	{
		if (m_isSimulating)
		{
			return;
		}

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
		{
			return;
		}

		for (Entity e : toDelete)
		{
			removeBall(e);
		}
	}

	void spawnBallAtMouse()
	{
		pushUndoState();
		invalidatePlaySnapshotIfSimulating();

		auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		vec2 world = ECS::Camera::screenPositionToWorldPosition2D(
			cam, vec2(m_tempSvc->input().getMouseX(), m_tempSvc->input().getMouseY()));

		world = computeNonOverlappingPlacement(world, world, static_cast<Entity>(-1));

		createBallEntity(world, static_cast<unsigned int>(m_selectedMaterial), false);
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

	bool isCellOccupied(vec2 cell, Entity exclude) const
	{
		constexpr float MIN_SEP_SQ = (BALL_DIAMETER - 0.02f) * (BALL_DIAMETER - 0.02f);
		for (const auto& b : m_balls)
		{
			if (b.entity == exclude || !hasTransform(b.entity))
			{
				continue;
			}
			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			const vec2 p(t.position.x, t.position.y);
			if (glm::length2(p - cell) < MIN_SEP_SQ)
			{
				return true;
			}
		}
		return false;
	}

	float minBallDistanceSqToOthers(vec2 pos, Entity excludeBall) const
	{
		float minSq = (std::numeric_limits<float>::max)();
		for (const auto& b : m_balls)
		{
			if (b.entity == excludeBall || !hasTransform(b.entity))
			{
				continue;
			}
			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			const vec2 other(t.position.x, t.position.y);
			minSq = (std::min)(minSq, glm::length2(pos - other));
		}
		return minSq;
	}

	vec2 projectOutOfBalls(vec2 pos, vec2 fallbackFrom, Entity excludeBall, int sweeps = 12) const
	{
		constexpr float MIN_DIST_SQ = BALL_DIAMETER * BALL_DIAMETER;
		for (int s = 0; s < sweeps; ++s)
		{
			bool anyPushed = false;
			for (size_t i = 0; i < m_balls.size(); ++i)
			{
				const Entity otherEnt = m_balls[i].entity;
				if (otherEnt == excludeBall || !hasTransform(otherEnt))
				{
					continue;
				}
				const auto& t = m_tempRegistry->getComponent<Transform>(otherEnt);
				const vec2 other(t.position.x, t.position.y);
				const vec2 diff = pos - other;
				const float distSq = glm::length2(diff);
				if (distSq >= MIN_DIST_SQ - 1e-6f)
				{
					continue;
				}
				anyPushed = true;

				vec2 dir(1.0f, 0.0f);
				if (distSq >= 1e-8f)
				{
					dir = diff / std::sqrt(distSq);
				}
				else
				{
					const vec2 fbDiff = fallbackFrom - other;
					const float fbLenSq = glm::length2(fbDiff);
					if (fbLenSq >= 1e-8f)
					{
						dir = fbDiff / std::sqrt(fbLenSq);
					}
					else
					{
						const float angle = static_cast<float>(i) * 2.3999632f;
						dir = vec2(std::cos(angle), std::sin(angle));
					}
				}
				pos = other + dir * BALL_DIAMETER;
			}
			if (!anyPushed)
			{
				break;
			}
		}
		return pos;
	}

	vec2 resolveBallDragNonOverlap(vec2 candidate, vec2 fallbackFrom, Entity excludeBall) const
	{
		constexpr float VALID_DIST_SQ = (BALL_DIAMETER - 2e-3f) * (BALL_DIAMETER - 2e-3f);
		if (m_balls.empty())
		{
			return candidate;
		}

		const vec2 projected = projectOutOfBalls(candidate, fallbackFrom, excludeBall);
		if (minBallDistanceSqToOthers(projected, excludeBall) >= VALID_DIST_SQ)
		{
			return projected;
		}

		// Candidate landed inside a tight pocket; bisect along the move segment so surface sliding stops cleanly.
		if (minBallDistanceSqToOthers(fallbackFrom, excludeBall) >= VALID_DIST_SQ)
		{
			vec2 best = fallbackFrom;
			float lo = 0.0f;
			float hi = 1.0f;
			for (int step = 0; step < 5; ++step)
			{
				const float mid = 0.5f * (lo + hi);
				const vec2 midProj = projectOutOfBalls(glm::mix(fallbackFrom, candidate, mid), best, excludeBall);
				if (minBallDistanceSqToOthers(midProj, excludeBall) >= VALID_DIST_SQ)
				{
					best = midProj;
					lo = mid;
				}
				else
				{
					hi = mid;
				}
			}
			return best;
		}

		// Previously overlapped ball: keep whichever projection creates more clearance.
		const vec2 fallbackProj = projectOutOfBalls(fallbackFrom, fallbackFrom, excludeBall);
		return (minBallDistanceSqToOthers(projected, excludeBall) >=
				minBallDistanceSqToOthers(fallbackProj, excludeBall))
				   ? projected
				   : fallbackProj;
	}

	vec2 computeNonOverlappingPlacement(vec2 worldPos, vec2 fallbackFrom, Entity excludeBall)
	{
		if (gridEnabled())
		{
			const vec2 snapped = snapToGrid(worldPos, excludeBall);
			if (!isCellOccupied(snapped, excludeBall))
			{
				return snapped;
			}
			return resolveBallDragNonOverlap(snapped, fallbackFrom, excludeBall);
		}
		return resolveBallDragNonOverlap(worldPos, fallbackFrom, excludeBall);
	}

	bool isMouseInViewport() const
	{
		if (m_tempSvc == nullptr || ImGui::GetIO().WantCaptureMouse)
		{
			return false;
		}

		const float winW = static_cast<float>((std::max)(1, Display::width));
		const float winH = static_cast<float>((std::max)(1, Display::height));
		const float mx = m_tempSvc->input().getMouseX();
		const float myTopLeft = winH - m_tempSvc->input().getMouseY();
		return mx >= 0.0f && mx < (winW - PANEL_WIDTH) && myTopLeft >= m_menuBarHeight && myTopLeft < winH;
	}

	void toggleFixHoveredOrSelectedBall()
	{
		Entity target = isMouseInViewport() ? pickBallAtMouse() : static_cast<Entity>(-1);
		if (!isValidEntityId(target))
		{
			target = m_tagSelectedEntity;
		}
		if (!isValidEntityId(target) || m_tempRegistry == nullptr || !m_tempRegistry->hasComponent<RigidBody2D>(target))
		{
			return;
		}

		pushUndoState();
		auto& rb = m_tempRegistry->getComponent<RigidBody2D>(target);
		rb.isFixed = !rb.isFixed;
		m_tempRegistry->setComponentDirty(rb);
	}

	void handleCameraInput()
	{
		const bool inViewport = isMouseInViewport();
		if (inViewport)
		{
			float wheel = 0.0f;
			if (m_tempSvc->input().getMouseButtonDown(Input::WheelUp))
				wheel += 1.0f;
			if (m_tempSvc->input().getMouseButtonDown(Input::WheelDown))
				wheel -= 1.0f;
			if (wheel != 0.0f)
				zoomCamera(wheel);
		}

		const bool middleDown = m_tempSvc->input().getMouseButton(Input::MiddleClick);
		if (middleDown && m_middleWasDown)
		{
			panCamera(vec2(m_tempSvc->input().getMouseDeltaXRaw(), -m_tempSvc->input().getMouseDeltaYRaw()));
		}
		m_middleWasDown = middleDown && (m_middleWasDown || inViewport);
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
			if (isMouseInViewport())
			{
				m_constraintStartBall = static_cast<Entity>(-1);
				startRightDrag();
				m_rightWasDown = true;
			}
			return;
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
		const SimulationID simulationId =
			(hit == static_cast<Entity>(-1)) ? INVALID_SIMULATION_ID : getSimulationId(hit);

		if (hit == static_cast<Entity>(-1) || simulationId == INVALID_SIMULATION_ID)
		{
			// Nothing under the cursor: the right button always drags the view.
			m_dragMode = DragMode::Camera;
			return;
		}

		m_dragMode = DragMode::Ball;
		m_draggedBall = hit;
		m_draggedSimulationId = simulationId;
		if (m_isRelaxing)
		{
			m_isRelaxing = false;
			m_relaxationVelocities.clear();
		}

		auto& rb = m_tempRegistry->getComponent<RigidBody2D>(m_draggedBall);
		auto& t = m_tempRegistry->getComponent<Transform>(m_draggedBall);
		m_wasFixedBeforeDrag = rb.isFixed;
		m_resizedLinksDuringBallDrag = false;
		m_dragStartBallPos = vec2(t.position.x, t.position.y);
		m_pendingBallDragUndo = captureUndoState();
		m_hasPendingBallDragUndo = true;

		m_keepFixedAfterDrag = rb.isFixed;
		rb.isFixed = true;
		m_tempRegistry->setComponentDirty(rb);

		const vec2 startPos =
			computeNonOverlappingPlacement(getMouseWorldPosition(), m_dragStartBallPos, m_draggedBall);

		t.position = vec3(startPos.x, startPos.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		if (gridEnabled() || isCtrlHeld())
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

		if (m_dragMode != DragMode::Ball || m_draggedBall == static_cast<Entity>(-1) ||
			m_draggedSimulationId == INVALID_SIMULATION_ID)
			return;

		if (m_tempSvc->input().getKeyDown(Input::F))
		{
			m_keepFixedAfterDrag = !m_keepFixedAfterDrag;
		}

		auto& t = m_tempRegistry->getComponent<Transform>(m_draggedBall);
		const vec2 previousPosition(t.position.x, t.position.y);
		const vec2 dragPos = computeNonOverlappingPlacement(getMouseWorldPosition(), previousPosition, m_draggedBall);
		t.position = vec3(dragPos.x, dragPos.y, 0.0f);
		m_tempRegistry->setComponentDirty(t);

		if (isCtrlHeld() || (gridEnabled() && glm::length2(dragPos - previousPosition) > 0.0f))
		{
			// Snapping or holding Ctrl updates attached constraints to match the moved ball.
			resizeLinksForBall(m_draggedBall);
		}
	}

	void endRightDrag()
	{
		if (m_dragMode == DragMode::Ball && m_draggedBall != static_cast<Entity>(-1) &&
			m_draggedSimulationId != INVALID_SIMULATION_ID)
		{
			if (!m_keepFixedAfterDrag)
			{
				auto& rb = m_tempRegistry->getComponent<RigidBody2D>(m_draggedBall);
				rb.isFixed = false;
				m_tempRegistry->setComponentDirty(rb);
			}

			if (m_hasPendingBallDragUndo && hasTransform(m_draggedBall))
			{
				const auto& t = m_tempRegistry->getComponent<Transform>(m_draggedBall);
				const bool moved = glm::length2(vec2(t.position.x, t.position.y) - m_dragStartBallPos) > 1e-6f;
				const bool pinChanged = (m_keepFixedAfterDrag != m_wasFixedBeforeDrag);
				if (moved || pinChanged || m_resizedLinksDuringBallDrag)
				{
					commitUndoState(m_pendingBallDragUndo);
				}
			}
			m_hasPendingBallDragUndo = false;
			m_resizedLinksDuringBallDrag = false;

			m_draggedBall = static_cast<Entity>(-1);
			m_draggedSimulationId = INVALID_SIMULATION_ID;
			m_keepFixedAfterDrag = false;
		}

		m_dragMode = DragMode::None;
	}

	void handleLeftClickInput()
	{
		if (!m_tempSvc->input().getMouseButtonDown(Input::LeftClick) || m_tempSvc->input().isUIClick() ||
			!isMouseInViewport())
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
				handleConstraintClick();
				break;
			case ToolMode::Remove:
				handleRemoveClick();
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

	void handleLeftReleaseInput()
	{
		if ((m_toolMode != ToolMode::Spring && m_toolMode != ToolMode::Distance) ||
			!isValidEntityId(m_constraintStartBall))
		{
			return;
		}

		if (!m_tempSvc->input().getMouseButtonUp(Input::LeftClick) || m_tempSvc->input().isUIClick() ||
			!isMouseInViewport())
		{
			return;
		}

		const Entity hit = pickBallAtMouse();
		if (isValidEntityId(hit) && hit != m_constraintStartBall)
		{
			const LinkType type = (m_toolMode == ToolMode::Distance) ? LinkType::Distance : LinkType::Spring;
			addConstraintLink(m_constraintStartBall, hit, type);
			m_constraintStartBall = static_cast<Entity>(-1);
		}
	}

	void handleRemoveClick()
	{
		Entity hitBall = static_cast<Entity>(-1);
		int linkIndex = -1;
		resolveRemoveTarget(hitBall, linkIndex);

		if (isValidEntityId(hitBall))
		{
			pushUndoState();
			removeBall(hitBall);
			return;
		}

		if (linkIndex >= 0)
		{
			pushUndoState();
			removeLinkAt(static_cast<size_t>(linkIndex));
			m_tempRegistry->freeRemovedComponents();
		}
	}

	void handleDeleteKey()
	{
		if (isMouseInViewport())
		{
			Entity hitBall = static_cast<Entity>(-1);
			int linkIndex = -1;
			resolveRemoveTarget(hitBall, linkIndex);

			if (isValidEntityId(hitBall))
			{
				pushUndoState();
				removeBall(hitBall);
				return;
			}

			if (linkIndex >= 0)
			{
				pushUndoState();
				removeLinkAt(static_cast<size_t>(linkIndex));
				m_tempRegistry->freeRemovedComponents();
				return;
			}
		}

		if (!m_selectedLinkConstraints.empty())
		{
			deleteSelectedLinks();
			return;
		}

		if (isValidEntityId(m_tagSelectedEntity))
		{
			pushUndoState();
			removeBall(m_tagSelectedEntity);
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
			const LinkType type = (m_toolMode == ToolMode::Distance) ? LinkType::Distance : LinkType::Spring;
			addConstraintLink(m_constraintStartBall, hit, type);
		}

		m_constraintStartBall = static_cast<Entity>(-1);
	}

	Entity pickBallAtMouse(float* outDistance = nullptr)
	{
		const vec2 world = getMouseWorldPosition();
		float best = ballPickRadiusWorld();
		Entity bestEntity = static_cast<Entity>(-1);

		for (const auto& b : m_balls)
		{
			if (!hasTransform(b.entity))
				continue;

			const auto& t = m_tempRegistry->getComponent<Transform>(b.entity);
			const vec2 p(t.position.x, t.position.y);
			const float d = length(world - p);
			if (d < best)
			{
				best = d;
				bestEntity = b.entity;
			}
		}

		if (outDistance != nullptr)
		{
			*outDistance = best;
		}
		return bestEntity;
	}

	void resolveRemoveTarget(Entity& outBall, int& outLinkIdx)
	{
		outBall = static_cast<Entity>(-1);
		outLinkIdx = -1;

		if (m_tempSvc == nullptr || m_tempRegistry == nullptr)
		{
			return;
		}

		const bool shiftHeld = m_tempSvc->input().getKey(Input::LeftShift);
		const RemoveTarget effective =
			(m_removeTarget == RemoveTarget::Auto && shiftHeld) ? RemoveTarget::Links : m_removeTarget;

		if (effective == RemoveTarget::Balls)
		{
			outBall = pickBallAtMouse();
			return;
		}

		if (effective == RemoveTarget::Links)
		{
			outLinkIdx = pickLinkAtMouse();
			return;
		}

		float ballDist = 0.0f;
		const Entity hitBall = pickBallAtMouse(&ballDist);

		float linkDist = 0.0f;
		bool linkInsideSpan = false;
		const int hitLink = pickLinkAtMouse(&linkDist, &linkInsideSpan);

		const float coreRadius = (std::min)(BALL_CORE_RADIUS, BALL_WORLD_RADIUS * 0.6f);
		if (isValidEntityId(hitBall) && ballDist <= coreRadius)
		{
			outBall = hitBall;
			return;
		}

		if (hitLink >= 0 && (linkInsideSpan || !isValidEntityId(hitBall)))
		{
			outLinkIdx = hitLink;
			return;
		}

		outBall = hitBall;
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

		SimulationID idA = getSimulationId(a);
		SimulationID idB = getSimulationId(b);
		if (idA == INVALID_SIMULATION_ID || idB == INVALID_SIMULATION_ID)
			return;

		pushUndoState();
		invalidatePlaySnapshotIfSimulating();

		auto& ta = m_tempRegistry->getComponent<Transform>(a);
		auto& tb = m_tempRegistry->getComponent<Transform>(b);
		vec2 pa(ta.position.x, ta.position.y);
		vec2 pb(tb.position.x, tb.position.y);
		float restDistance = (std::clamp)(length(pb - pa), minAllowedLinkDistance(type), MAX_LINK_DISTANCE);

		Entity constraintEnt = createConstraintComponentEntity(a, b, type, restDistance, SPRING_STIFFNESS);
		if (constraintEnt == INVALID_ENTITY)
		{
			return;
		}

		m_links.push_back({a, b, idA, idB, restDistance, type, constraintEnt});
	}

	void removeConstraintLink(Entity a, Entity b)
	{
		for (size_t i = 0; i < m_links.size();)
		{
			DistanceLink& link = m_links[i];
			bool sameDir = (link.a == a && link.b == b);
			bool reverseDir = (link.a == b && link.b == a);
			if (sameDir || reverseDir)
			{
				removeLinkAt(i);
			}
			else
			{
				++i;
			}
		}
		m_tempRegistry->freeRemovedComponents();
	}

	void handleLinkInteraction()
	{
		const bool freshClick = m_tempSvc->input().getMouseButtonDown(Input::LeftClick) &&
								!m_tempSvc->input().isUIClick() && isMouseInViewport();

		if (freshClick && m_toolMode == ToolMode::Modify)
		{
			m_modifyPressPending = true;
			m_modifyDragActive = false;
			m_modifyPressMouseX = m_tempSvc->input().getMouseX();
			m_modifyPressMouseY = m_tempSvc->input().getMouseY();
			const int index = pickLinkAtMouse();
			if (index >= 0)
			{
				DistanceLink& picked = m_links[static_cast<size_t>(index)];
				m_pendingLinkDragUndo = captureUndoState();
				m_hasPendingLinkDragUndo = true;
				m_modifyPressedLink = picked.constraintEntity;
				m_linkDragStartX = m_modifyPressMouseX;
				m_linkDragStartDist = picked.restDistance;
			}
			else
			{
				m_modifyPressedLink = static_cast<Entity>(-1);
				m_hasPendingLinkDragUndo = false;
			}
		}

		if (!m_tempSvc->input().getMouseButton(Input::LeftClick))
		{
			if (m_modifyDragActive)
			{
				DistanceLink* released = findLinkByConstraint(m_draggedLinkConstraint);
				if (released != nullptr && std::abs(released->restDistance - m_linkDragStartDist) > 1e-4f)
				{
					commitUndoState(m_pendingLinkDragUndo);
				}
				m_hasPendingLinkDragUndo = false;
			}
			else if (m_modifyPressPending)
			{
				m_hasPendingLinkDragUndo = false;
				const bool shiftHeld = m_tempSvc->input().getKey(Input::LeftShift);
				if (isValidEntityId(m_modifyPressedLink))
				{
					if (shiftHeld)
					{
						toggleLinkSelection(m_modifyPressedLink);
					}
					else
					{
						selectSingleLink(m_modifyPressedLink);
					}
				}
				else if (!shiftHeld)
				{
					clearLinkSelection();
				}
			}

			m_modifyPressPending = false;
			m_modifyDragActive = false;
			m_modifyPressedLink = static_cast<Entity>(-1);
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			return;
		}

		if (!m_modifyPressPending || !isValidEntityId(m_modifyPressedLink))
		{
			return;
		}

		const float mouseX = m_tempSvc->input().getMouseX();
		const float mouseY = m_tempSvc->input().getMouseY();
		if (!m_modifyDragActive)
		{
			const float moveSq = (mouseX - m_modifyPressMouseX) * (mouseX - m_modifyPressMouseX) +
								 (mouseY - m_modifyPressMouseY) * (mouseY - m_modifyPressMouseY);
			if (moveSq <= MODIFY_DRAG_THRESHOLD_PX * MODIFY_DRAG_THRESHOLD_PX)
			{
				return;
			}

			m_modifyDragActive = true;
			m_draggedLinkConstraint = m_modifyPressedLink;
			if (!isLinkSelected(m_draggedLinkConstraint))
			{
				selectSingleLink(m_draggedLinkConstraint);
			}
		}

		DistanceLink* dragged = findLinkByConstraint(m_draggedLinkConstraint);
		if (dragged == nullptr)
		{
			m_modifyPressPending = false;
			m_modifyDragActive = false;
			m_modifyPressedLink = static_cast<Entity>(-1);
			m_draggedLinkConstraint = static_cast<Entity>(-1);
			return;
		}

		const bool fineMode = m_tempSvc->input().getKey(Input::LeftShift);
		const float scale = fineMode ? 0.008f : 0.03f;
		const float snapDiv = fineMode ? 100.0f : 20.0f;
		const float dx = (mouseX - m_linkDragStartX) * scale;
		float newDist = std::round((m_linkDragStartDist + dx) * snapDiv) / snapDiv;
		newDist = (std::clamp)(newDist, minAllowedLinkDistance(dragged->type), MAX_LINK_DISTANCE);
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

	int pickLinkAtMouse(float* outDistance = nullptr, bool* outInsideSpan = nullptr)
	{
		const vec2 mouse = getMouseWorldPosition();
		float best = linkPickRadiusWorld();
		int bestIndex = -1;
		bool bestInside = false;

		for (size_t i = 0; i < m_links.size(); ++i)
		{
			const DistanceLink& link = m_links[i];
			if (!hasTransform(link.a) || !hasTransform(link.b))
			{
				continue;
			}

			const auto& ta = m_tempRegistry->getComponent<Transform>(link.a);
			const auto& tb = m_tempRegistry->getComponent<Transform>(link.b);
			bool inside = false;
			const float d = distanceToSegment(mouse, vec2(ta.position.x, ta.position.y),
											  vec2(tb.position.x, tb.position.y), inside);
			if (d < best)
			{
				best = d;
				bestIndex = static_cast<int>(i);
				bestInside = inside;
			}
		}

		if (outDistance != nullptr)
		{
			*outDistance = best;
		}
		if (outInsideSpan != nullptr)
		{
			*outInsideSpan = bestInside;
		}
		return bestIndex;
	}

	float worldPerPixel() const
	{
		if (m_tempRegistry == nullptr || m_tempSvc == nullptr)
		{
			return 0.05f;
		}
		const auto& cam = m_tempRegistry->getComponent<Transform>(m_tempSvc->render().getCameraEntity());
		const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
		return cam.position.z / halfHeight;
	}

	float ballPickRadiusWorld() const
	{
		return (std::max)(BALL_HIT_RADIUS, BALL_PICK_MIN_PIXELS * worldPerPixel());
	}

	float linkPickRadiusWorld() const
	{
		return (std::min)(LINK_PICK_MAX_WORLD, (std::max)(0.16f, LINK_PICK_PIXELS * worldPerPixel()));
	}

	static float distanceToSegment(vec2 p, vec2 a, vec2 b, bool& outInsideSpan)
	{
		const vec2 ab = b - a;
		const float lengthSq = glm::length2(ab);
		if (lengthSq <= 0.0001f)
		{
			outInsideSpan = false;
			return glm::length(p - a);
		}

		const float proj = glm::dot(p - a, ab) / lengthSq;
		outInsideSpan = (proj > 0.0f && proj < 1.0f);
		const float t = (std::clamp)(proj, 0.0f, 1.0f);
		return glm::length(p - (a + ab * t));
	}

	SimulationID getSimulationId(Entity e)
	{
		if (e != static_cast<Entity>(-1) && m_tempRegistry && m_tempRegistry->hasComponent<RigidBody2D>(e))
		{
			return m_tempRegistry->getComponent<RigidBody2D>(e).simulationId;
		}
		return INVALID_SIMULATION_ID;
	}

	void refreshSimulationIds()
	{
		if (!m_tempRegistry)
		{
			return;
		}

		for (auto& b : m_balls)
		{
			if (m_tempRegistry->hasComponent<RigidBody2D>(b.entity))
			{
				b.simulationId = m_tempRegistry->getComponent<RigidBody2D>(b.entity).simulationId;
			}
		}
		for (auto& link : m_links)
		{
			if (m_tempRegistry->hasComponent<RigidBody2D>(link.a))
			{
				link.simulationIdA = m_tempRegistry->getComponent<RigidBody2D>(link.a).simulationId;
			}
			if (m_tempRegistry->hasComponent<RigidBody2D>(link.b))
			{
				link.simulationIdB = m_tempRegistry->getComponent<RigidBody2D>(link.b).simulationId;
			}
		}
	}

	// -----------------------------------------------------------------------
	// Tag editor UI
	// -----------------------------------------------------------------------

	void hideTagRing()
	{
		if (m_tempRegistry == nullptr)
		{
			return;
		}
		if (isValidEntityId(m_tagCircleOuter) && hasUIShape(m_tagCircleOuter))
		{
			auto& outer = m_tempRegistry->getComponent<UIShape>(m_tagCircleOuter);
			outer.parameters[0] = -10000.0f;
			outer.parameters[1] = -10000.0f;
			outer.parameters[2] = 0.0f;
		}
		if (isValidEntityId(m_tagCircleInner) && hasUIShape(m_tagCircleInner))
		{
			auto& inner = m_tempRegistry->getComponent<UIShape>(m_tagCircleInner);
			inner.parameters[0] = -10000.0f;
			inner.parameters[1] = -10000.0f;
			inner.parameters[2] = 0.0f;
		}
	}

	void clearTagSelection()
	{
		m_tagSelectedEntity = static_cast<Entity>(-1);
		m_tagBufferEntity = static_cast<Entity>(-1);
		m_tagBuf[0] = '\0';
		m_newTagBuf[0] = '\0';
		hideTagRing();
	}

	void setToolMode(ToolMode mode)
	{
		if (m_toolMode == mode)
		{
			return;
		}
		m_toolMode = mode;
		m_constraintStartBall = static_cast<Entity>(-1);
		m_modifyPressPending = false;
		m_modifyDragActive = false;
		m_modifyPressedLink = static_cast<Entity>(-1);
		m_draggedLinkConstraint = static_cast<Entity>(-1);
		m_hasPendingLinkDragUndo = false;
		if (mode != ToolMode::TagEditor)
		{
			clearTagSelection();
		}
	}

	void updateTagEditor()
	{
		// Only show the selection ring while the Tag Editor tool is active
		if (m_toolMode != ToolMode::TagEditor || !isValidEntityId(m_tagSelectedEntity))
		{
			hideTagRing();
			return;
		}

		if (!hasTransform(m_tagSelectedEntity))
		{
			clearTagSelection();
			return;
		}

		// Lazy-initialize the ring indicator shapes on first use
		if (m_tagCircleOuter == static_cast<Entity>(-1))
		{
			float p[8]{};
			m_tagCircleOuter = m_tempSvc->shapes().addUIShape({.shapeId = DefaultShapes::Circle,
															   .variables = p,
															   .material = 7,
															   .combination = CombinationType::Addition,
															   .group = TAG_RING_GROUP});
			m_tempSvc->serialization().blacklistEntity(m_tagCircleOuter);

			m_tagCircleInner = m_tempSvc->shapes().addUIShape({.shapeId = DefaultShapes::Circle,
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
		const Entity picked = pickBallAtMouse();
		if (!isValidEntityId(picked))
		{
			clearTagSelection();
			return;
		}
		m_tagSelectedEntity = picked;
	}

	void paintBallAtMouse()
	{
		Entity hit = pickBallAtMouse();
		if (!isValidEntityId(hit))
		{
			return;
		}

		auto& dot = m_tempRegistry->getComponent<Dot>(hit);
		const unsigned int nextMaterial = static_cast<unsigned int>(m_selectedMaterial);
		if (dot.materialId == nextMaterial)
		{
			return;
		}

		pushUndoState();
		dot.materialId = nextMaterial;
	}

	bool hasUIShape(Entity e)
	{
		auto arr = m_tempRegistry->getComponentArray<UIShape>();
		return arr->hasData(e);
	}

	void loadCluster(const std::string& path)
	{
		if (!std::filesystem::exists(path))
		{
			WeirdEngine::Logger::error("[ClusterEditor] Cannot load cluster: file does not exist: " + path);
			return;
		}

		clearCluster();
		clearUndoHistory();

		m_currentFilePath = path;
		std::string stem = std::filesystem::path(path).stem().string();
		if (!stem.empty())
		{
			std::snprintf(m_fileNameBuf, sizeof(m_fileNameBuf), "%s", stem.c_str());
		}

		// Remember constraint count before loading so we can find new ones
		size_t prevSpringCount = m_tempRegistry->getComponentArray<Spring>()->getSize();
		size_t prevDistCount = m_tempRegistry->getComponentArray<DistanceConstraint>()->getSize();

		// Load the file — creates new entities / rigid bodies / constraints
		NameMap loadedTags = m_tempSvc->serialization().loadWeirdFile(path);

		// Apply loaded tags to the scene
		for (const auto& [name, entity] : loadedTags)
		{
			m_tempSvc->names().setName(entity, name);
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
		std::unordered_map<SimulationID, Entity> simIdToEntity;

		for (size_t i = 0; i < rbArray->getSize(); i++)
		{
			Entity e = rbArray->getEntityAtIdx(i);
			if (existingBalls.count(e))
				continue;
			if (!dotArray->hasData(e))
				continue;

			auto& rb = rbArray->getDataAtIdx(i);
			SimulationID simId = rb.simulationId;
			m_balls.push_back({e, simId});
			simIdToEntity[simId] = e;
		}

		// Collect constraints
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

			SimulationID idA = m_tempRegistry->getComponent<RigidBody2D>(a).simulationId;
			SimulationID idB = m_tempRegistry->getComponent<RigidBody2D>(b).simulationId;
			m_links.push_back({a, b, idA, idB, spring.restDistance, LinkType::Spring, springEnt});
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

			SimulationID idA = m_tempRegistry->getComponent<RigidBody2D>(a).simulationId;
			SimulationID idB = m_tempRegistry->getComponent<RigidBody2D>(b).simulationId;
			m_links.push_back({a, b, idA, idB, dist.distance, LinkType::Distance, distEnt});
			newDists++;
		}

		if (m_menuBarHeight > 0.0f)
		{
			frameCluster();
		}

		std::string loadMsg = "[ClusterEditor] Loaded " + std::to_string(simIdToEntity.size()) + " balls and " +
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
