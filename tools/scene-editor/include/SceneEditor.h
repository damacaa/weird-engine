#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <imgui.h>
#include <json/json.h>
#include <SDL3/SDL_dialog.h>

#include "EditorProject.h"
#include "weird-engine/components/FlyMovement.h"
#include "weird-engine/components/FlyMovement2D.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/services/ServiceProvider.h"
#include "weird-engine/shapes/NodeGraph.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-engine/shapes/NodeGraphPresets.h"
#include "weird-engine/shapes/NodeGraphSerializer.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h>
#endif
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/Light2DComponent.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/Shape.h"
#include "weird-renderer/components/TextRenderer.h"
#include <weird-engine.h>

extern WeirdEngine::vec3 g_cameraPositon;

namespace WeirdEngine
{
	class SceneEditor : public Scene2D
	{
	public:
		SceneEditor() = default;

	private:
		// =====================================================================
		// State
		// =====================================================================
		Registry* m_registry = nullptr;
		ServiceProvider* m_services = nullptr;

		Editor::EditorProject m_project;

		std::string m_currentScenePath;
		bool m_isSceneDirty = false;

		// Selection state (supports single and multi-selection)
		Entity m_selectedEntity = INVALID_ENTITY;
		uint32_t m_selectedClusterId = 0;
		uint32_t m_selectedGroupId = 0;
		std::unordered_set<Entity> m_selectedEntities;
		std::unordered_set<uint32_t> m_selectedClusterIds;
		bool m_selectIndividualClusterEntities = false;

		// Hierarchy Item reference for range selection (Shift-click)
		enum class HierarchyItemType
		{
			Entity,
			Cluster,
			Group
		};

		struct HierarchyItem
		{
			HierarchyItemType type = HierarchyItemType::Entity;
			uint32_t id = 0;

			bool operator==(const HierarchyItem& other) const
			{
				return type == other.type && id == other.id;
			}
		};

		std::vector<HierarchyItem> m_visibleHierarchyItems;
		HierarchyItem m_shiftAnchorItem{HierarchyItemType::Entity, 0};
		bool m_hasShiftAnchor = false;
		HierarchyItem m_pendingShiftTarget{HierarchyItemType::Entity, 0};
		bool m_pendingShiftIsCtrl = false;
		bool m_hasPendingShift = false;

		// Groups (visual organizational containers in the hierarchy)
		struct EditorGroup
		{
			uint32_t id = 0;
			std::string name = "New Group";
			std::vector<Entity> entities;
			std::vector<uint32_t> clusterIds;
		};
		std::vector<EditorGroup> m_groups;
		uint32_t m_nextGroupId = 1;

		// Cluster rest & reset data
		struct ClusterRestData
		{
			vec2 initialPosition{0.0f, 0.0f};
			float initialRotation{0.0f};
			std::vector<vec2> restOffsets;
		};
		std::unordered_map<uint32_t, ClusterRestData> m_clusterRestData;

		// Drag state
		bool m_isDragging = false;
		vec2 m_dragStartMouseWorld{0.0f, 0.0f};
		vec2 m_dragStartPos{0.0f, 0.0f};
		std::unordered_map<Entity, vec2> m_dragEntityStartPositions;
		std::unordered_map<uint32_t, vec2> m_dragClusterStartPositions;

		// Camera control state
		bool m_middleWasDown = false;

		// Grid & Snapping
		bool m_gridEnabled = true;
		bool m_gridSnap = false;
		float m_gridSize = 1.0f;

		// Simulation state
		bool m_isSimulating = false;
		bool m_hasSnapshot = false;
		float m_playGravity = -9.8f;
		float m_playDamping = 0.001f;

		// UI Panel Layout
		float m_menuBarHeight = 26.0f;
		static constexpr float LEFT_PANEL_WIDTH = 320.0f;
		static constexpr float RIGHT_PANEL_WIDTH = 340.0f;
		static constexpr float BOTTOM_BAR_HEIGHT = 38.0f;

		// Asset Browser Filter
		int m_assetFilterType = 0; // 0 = All, 1 = Scenes, 2 = Clusters, 3 = Shapes
		char m_assetSearchBuf[64] = "";
		char m_hierarchySearchBuf[64] = "";

		// Inspector text buffers
		char m_entityNameBuf[64] = "";
		char m_clusterNameBuf[64] = "";
		char m_newTagBuf[64] = "";
		uint32_t m_renamingGroupId = 0;
		char m_renameGroupBuf[128] = "";

		// Materials editor state
		bool m_selectMaterialsTab = false;
		int m_selectedMaterialSlot = 0;
		vec4 m_copiedMaterialColor = vec4(1.0f);
		bool m_hasCopiedMaterialColor = false;

		// Asset creation & tool launch state
		char m_newClusterNameBuf[64] = "NewCluster";
		char m_newShapeNameBuf[64] = "NewShape";
		bool m_openNewClusterModal = false;
		bool m_openNewShapeModal = false;

		// File dialog states
		std::mutex m_fileMutex;
		std::string m_pendingLoadScenePath;
		std::string m_pendingSaveScenePath;

		// Asset hot-reloading
		std::chrono::steady_clock::time_point m_lastAssetPollTime = std::chrono::steady_clock::now();
		std::unordered_map<std::string, std::filesystem::file_time_type> m_clusterAssetWriteTimes;

		// Viewport Creation Context Menu
		bool m_openViewportContextMenu = false;
		vec2 m_contextMenuWorldPos{0.0f, 0.0f};

		// =====================================================================
		// Lifecycle
		// =====================================================================
		void onStart(Registry& registry, ServiceProvider& services) override
		{
			m_registry = &registry;
			m_services = &services;

			m_registry->registerComponent<Light2DComponent>();

			// Turn OFF debug input so clicking in the viewport does NOT spawn physics test balls!
			m_services->debug().setDebugInput(false);
			m_services->debug().setDebugFly(false);
			m_simulation2D.resume();
			m_simulation2D.setGravity(0.0f);
			m_simulation2D.setDamping(1.0f);

			services.render().getBackground().parallax = 1.0f;

			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			cam.position = g_cameraPositon;

			// Initialize default materials if none exist
			for (size_t i = 0; i < 16; ++i)
			{
				auto& mat = m_services->materials2D().createMaterial("mat_" + std::to_string(i));
				if (i < ColorPalette::Default.size())
				{
					mat.color = vec4(ColorPalette::Default[i], 1.0f);
				}
			}

			// Load project configuration
			m_project.loadConfig();

			// Auto-open last project or fallback to examples/sample-scenes
			if (!m_project.getLastProject().empty() && std::filesystem::exists(m_project.getLastProject()))
			{
				m_project.open(m_project.getLastProject());
			}
			else if (std::filesystem::exists("examples/sample-scenes/assets"))
			{
				m_project.open("examples/sample-scenes");
			}
			else if (std::filesystem::exists("../examples/sample-scenes/assets"))
			{
				m_project.open("../examples/sample-scenes");
			}
			else if (std::filesystem::exists("../../examples/sample-scenes/assets"))
			{
				m_project.open("../../examples/sample-scenes");
			}

			syncAllConstraintsClusterMembership();
		}

		void onUpdate(Registry& registry, ServiceProvider& services) override
		{
			m_registry = &registry;
			m_services = &services;

			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			g_cameraPositon = cam.position;

			m_project.pollDialogs();
			pollFileActions();

			auto now = std::chrono::steady_clock::now();
			if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAssetPollTime).count() >= 250)
			{
				m_lastAssetPollTime = now;
				pollAssetModifications();
			}

			handleHotkeys();
			handleCameraInput();
			handleViewportMouseInput();
		}

		void onCustomUI(Registry& registry, ServiceProvider& services) override
		{
			m_registry = &registry;
			m_services = &services;

			renderMenuBar();
			renderLeftPanel();
			renderInspector();
			renderBottomToolbar();
			renderProjectModal();
			renderViewportContextMenu();
		}

		// =====================================================================
		// Viewport & Input Interaction
		// =====================================================================
		void handleHotkeys()
		{
			ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureKeyboard)
			{
				return;
			}

			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::N))
			{
				newScene();
			}
			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::O))
			{
				openLoadSceneDialog();
			}
			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::S))
			{
				saveScenePrompt();
			}
			if (m_services->input().getKeyDown(Input::Space))
			{
				toggleSimulation();
			}
			if (m_services->input().getKeyDown(Input::Delete) || m_services->input().getKeyDown(Input::X))
			{
				deleteSelected();
			}
			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKey(Input::LeftShift) &&
				m_services->input().getKeyDown(Input::G))
			{
				ungroupSelected();
			}
			else if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::G))
			{
				groupSelected();
			}
			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::D))
			{
				duplicateSelected();
			}
			if (m_services->input().getKeyDown(Input::Esc))
			{
				deselectAll();
			}
			if (m_services->input().getKeyDown(Input::F))
			{
				resetCamera();
			}
			if (m_services->input().getKeyDown(Input::C))
			{
				m_selectIndividualClusterEntities = !m_selectIndividualClusterEntities;
			}
			if (m_services->input().getKeyDown(Input::R) && m_selectedClusterId != 0)
			{
				auto* c = m_services->clusters().getById(m_selectedClusterId);
				if (c)
				{
					resetClusterToInitialPosition(*c);
				}
			}
		}

		void handleCameraInput()
		{
			ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureMouse)
			{
				return;
			}

			// Zoom with scroll wheel
			float wheel = 0.0f;
			if (m_services->input().getMouseButtonDown(Input::WheelUp))
			{
				wheel += 1.0f;
			}
			if (m_services->input().getMouseButtonDown(Input::WheelDown))
			{
				wheel -= 1.0f;
			}
			if (wheel != 0.0f)
			{
				zoomCamera(wheel);
			}

			// Middle-mouse drag pan
			bool middleDown = m_services->input().getMouseButton(Input::MiddleClick);
			if (middleDown && m_middleWasDown)
			{
				panCamera(vec2(m_services->input().getMouseDeltaXRaw(), -m_services->input().getMouseDeltaYRaw()));
			}
			m_middleWasDown = middleDown;
		}

		void zoomCamera(float steps)
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			const float oldDist = cam.position.z;
			const float newDist = std::clamp(oldDist * std::pow(0.85f, steps), 5.0f, 250.0f);
			if (std::abs(newDist - oldDist) < 0.001f)
			{
				return;
			}

			const vec2 mouse(m_services->input().getMouseX(), m_services->input().getMouseY());
			const vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, mouse);
			const float halfHeight = (std::max)(1.0f, ImGui::GetIO().DisplaySize.y * 0.5f);
			const float scale = newDist / halfHeight;

			cam.position.z = newDist;
			cam.position.x = world.x - (mouse.x - ImGui::GetIO().DisplaySize.x * 0.5f) * scale;
			cam.position.y = world.y - (mouse.y - halfHeight) * scale;
			m_registry->setComponentDirty(cam);
		}

		void panCamera(vec2 screenDelta)
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			const float halfHeight = (std::max)(1.0f, ImGui::GetIO().DisplaySize.y * 0.5f);
			const float scale = cam.position.z / halfHeight;

			cam.position.x -= screenDelta.x * scale;
			cam.position.y -= screenDelta.y * scale;
			m_registry->setComponentDirty(cam);
		}

		void resetCamera()
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			cam.position = vec3(0.0f, 0.0f, 35.0f);
			cam.rotation = vec3(0.0f, 0.0f, -1.0f);
			m_registry->setComponentDirty(cam);
		}

		vec2 getMouseWorld()
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			return ECS::Camera::screenPositionToWorldPosition2D(
				cam, vec2(m_services->input().getMouseX(), m_services->input().getMouseY()));
		}

		void handleViewportMouseInput()
		{
			ImGuiIO& io = ImGui::GetIO();
			if (io.WantCaptureMouse)
			{
				if (m_isDragging)
				{
					m_isDragging = false;
				}
				return;
			}

			vec2 mouseWorld = getMouseWorld();

			// Left click down: selection & begin drag
			if (m_services->input().getMouseButtonDown(Input::LeftClick))
			{
				pickAtWorld(mouseWorld);
				if (hasSelection())
				{
					m_dragEntityStartPositions.clear();
					m_dragClusterStartPositions.clear();

					for (Entity e : m_selectedEntities)
					{
						m_dragEntityStartPositions[e] = getEntityPosition(e);
					}
					for (uint32_t cId : m_selectedClusterIds)
					{
						auto* c = m_services->clusters().getById(cId);
						if (c)
						{
							vec2 centroid = computeClusterCentroid(*c);
							resetClusterShapeAtPosition(*c, centroid);
							m_dragClusterStartPositions[cId] = centroid;
						}
					}
					if (m_selectedGroupId != 0)
					{
						auto* g = findGroup(m_selectedGroupId);
						if (g)
						{
							for (Entity e : g->entities)
							{
								m_dragEntityStartPositions[e] = getEntityPosition(e);
							}
							for (uint32_t cId : g->clusterIds)
							{
								auto* c = m_services->clusters().getById(cId);
								if (c)
								{
									vec2 centroid = computeClusterCentroid(*c);
									resetClusterShapeAtPosition(*c, centroid);
									m_dragClusterStartPositions[cId] = centroid;
								}
							}
						}
					}

					m_isDragging = true;
					m_dragStartMouseWorld = mouseWorld;
					m_dragStartPos = getSelectedPosition();
				}
			}

			// Left drag: move selected object
			if (m_isDragging && m_services->input().getMouseButton(Input::LeftClick))
			{
				vec2 delta = mouseWorld - m_dragStartMouseWorld;
				if (m_gridSnap && m_gridSize > 0.001f)
				{
					delta.x = std::round(delta.x / m_gridSize) * m_gridSize;
					delta.y = std::round(delta.y / m_gridSize) * m_gridSize;
				}

				if (!m_dragEntityStartPositions.empty() || !m_dragClusterStartPositions.empty())
				{
					for (const auto& [e, startPos] : m_dragEntityStartPositions)
					{
						setEntityPosition(e, startPos + delta);
					}
					for (const auto& [cId, startPos] : m_dragClusterStartPositions)
					{
						setClusterPosition(cId, startPos + delta);
					}
				}
				else
				{
					vec2 targetPos = m_dragStartPos + delta;
					setSelectedPosition(targetPos);
				}
				m_isSceneDirty = true;
			}

			if (m_isDragging && !m_services->input().getMouseButton(Input::LeftClick))
			{
				m_isDragging = false;
			}

			// Right-click in viewport: open creation context menu at clicked world position
			if (m_services->input().getMouseButtonDown(Input::RightClick))
			{
				m_contextMenuWorldPos = mouseWorld;
				m_openViewportContextMenu = true;
			}
		}

		void offsetShapeParameters(Shape& s, vec2 delta)
		{
			if (s.distanceFieldId == DefaultShapes::Line)
			{
				s.parameters[DefaultShapes::Line::StartX] += delta.x;
				s.parameters[DefaultShapes::Line::StartY] += delta.y;
				s.parameters[DefaultShapes::Line::EndX] += delta.x;
				s.parameters[DefaultShapes::Line::EndY] += delta.y;
				return;
			}

			const auto* customData = m_services->shapes().getCustomShapeData(s.distanceFieldId);
			if (customData)
			{
				int posXIdx = -1;
				int posYIdx = -1;
				for (int p = 0; p < 8; ++p)
				{
					std::string nameLower = customData->parameterNames[p];
					std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
					if (posXIdx == -1 &&
						(nameLower == "posx" || nameLower == "pos x" || nameLower == "pos_x" || nameLower == "x"))
					{
						posXIdx = p;
					}
					else if (posYIdx == -1 &&
							 (nameLower == "posy" || nameLower == "pos y" || nameLower == "pos_y" || nameLower == "y"))
					{
						posYIdx = p;
					}
				}
				if (posXIdx != -1 && posYIdx != -1)
				{
					s.parameters[posXIdx] += delta.x;
					s.parameters[posYIdx] += delta.y;
					return;
				}
			}

			// Default fallback for default shapes (and custom shapes using parameters 0 and 1 for position)
			s.parameters[0] += delta.x;
			s.parameters[1] += delta.y;
		}

		vec2 getShapePosition(const Shape& s, Entity e)
		{
			if (s.distanceFieldId == DefaultShapes::Line)
			{
				return vec2(
					(s.parameters[DefaultShapes::Line::StartX] + s.parameters[DefaultShapes::Line::EndX]) * 0.5f,
					(s.parameters[DefaultShapes::Line::StartY] + s.parameters[DefaultShapes::Line::EndY]) * 0.5f);
			}

			const auto* customData = m_services->shapes().getCustomShapeData(s.distanceFieldId);
			if (customData)
			{
				int posXIdx = -1;
				int posYIdx = -1;
				for (int p = 0; p < 8; ++p)
				{
					std::string nameLower = customData->parameterNames[p];
					std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
					if (posXIdx == -1 &&
						(nameLower == "posx" || nameLower == "pos x" || nameLower == "pos_x" || nameLower == "x"))
					{
						posXIdx = p;
					}
					else if (posYIdx == -1 &&
							 (nameLower == "posy" || nameLower == "pos y" || nameLower == "pos_y" || nameLower == "y"))
					{
						posYIdx = p;
					}
				}
				if (posXIdx != -1 && posYIdx != -1)
				{
					return vec2(s.parameters[posXIdx], s.parameters[posYIdx]);
				}
			}

			if (s.distanceFieldId < DefaultShapes::COUNT_2D)
			{
				return vec2(s.parameters[0], s.parameters[1]);
			}

			if (m_registry->hasComponent<Transform>(e))
			{
				return vec2(m_registry->getComponent<Transform>(e).position);
			}

			return vec2(s.parameters[0], s.parameters[1]);
		}

		std::filesystem::path resolveClusterPath(const std::string& assetPath)
		{
			std::filesystem::path p(assetPath);
			if (std::filesystem::exists(p))
				return p;

			if (m_project.isOpen())
			{
				if (std::filesystem::exists(m_project.getAssetsPath() / p))
					return m_project.getAssetsPath() / p;
				if (std::filesystem::exists(m_project.getAssetsPath() / p.filename()))
					return m_project.getAssetsPath() / p.filename();
			}

			if (std::filesystem::exists(std::filesystem::path("examples/sample-scenes/assets") / p.filename()))
				return std::filesystem::path("examples/sample-scenes/assets") / p.filename();

			return p;
		}

		vec2 computeClusterCentroid(const ClusterInstance& cluster)
		{
			vec2 centroid(0.0f);
			size_t count = 0;
			for (Entity e : cluster.entities)
			{
				if (m_registry->hasComponent<Transform>(e))
				{
					centroid += vec2(m_registry->getComponent<Transform>(e).position);
					count++;
				}
			}
			if (count > 0)
			{
				centroid /= static_cast<float>(count);
				return centroid;
			}
			return cluster.position;
		}

		ClusterRestData& getOrCreateClusterRestData(const ClusterInstance& cluster)
		{
			auto it = m_clusterRestData.find(cluster.id);
			if (it != m_clusterRestData.end())
			{
				return it->second;
			}

			ClusterRestData data;
			data.initialPosition = cluster.position;
			data.initialRotation = cluster.rotation;

			std::filesystem::path path = resolveClusterPath(cluster.assetPath);
			std::ifstream file(path);
			if (file.is_open())
			{
				try
				{
					json j;
					file >> j;
					if (j.contains("entities") && j["entities"].is_array())
					{
						std::vector<vec2> positions;
						vec2 centroid(0.0f);
						size_t count = 0;
						for (const auto& ej : j["entities"])
						{
							vec2 p(0.0f);
							if (ej.contains("transform") && ej["transform"].contains("position") &&
								ej["transform"]["position"].is_array() && ej["transform"]["position"].size() >= 2)
							{
								p.x = ej["transform"]["position"][0].get<float>();
								p.y = ej["transform"]["position"][1].get<float>();
								centroid += p;
								count++;
							}
							positions.push_back(p);
						}
						if (count > 0)
						{
							centroid /= static_cast<float>(count);
						}
						for (const auto& p : positions)
						{
							data.restOffsets.push_back(p - centroid);
						}
					}
				}
				catch (...)
				{
				}
			}

			// Fallback if file could not be read or had mismatched entity count
			if (data.restOffsets.empty() || data.restOffsets.size() != cluster.entities.size())
			{
				data.restOffsets.clear();
				vec2 centroid = computeClusterCentroid(cluster);
				for (Entity e : cluster.entities)
				{
					if (m_registry->hasComponent<Transform>(e))
					{
						vec2 p(m_registry->getComponent<Transform>(e).position);
						data.restOffsets.push_back(p - centroid);
					}
					else
					{
						data.restOffsets.push_back(vec2(0.0f));
					}
				}
			}

			auto [insIt, _] = m_clusterRestData.emplace(cluster.id, std::move(data));
			return insIt->second;
		}

		void resetClusterShapeAtPosition(ClusterInstance& cluster, vec2 targetPos)
		{
			auto& restData = getOrCreateClusterRestData(cluster);
			const auto& offsets = restData.restOffsets;

			float cosR = std::cos(cluster.rotation);
			float sinR = std::sin(cluster.rotation);

			cluster.position = targetPos;

			for (size_t i = 0; i < cluster.entities.size(); ++i)
			{
				Entity e = cluster.entities[i];
				vec2 rel = (i < offsets.size()) ? offsets[i] : vec2(0.0f);
				vec2 rotated = vec2(rel.x * cosR - rel.y * sinR, rel.x * sinR + rel.y * cosR);
				vec2 worldPos = targetPos + rotated;

				if (m_registry->hasComponent<Transform>(e))
				{
					auto& t = m_registry->getComponent<Transform>(e);
					t.position.x = worldPos.x;
					t.position.y = worldPos.y;
					t.position.z = 0.0f;
					m_registry->setComponentDirty(t);
				}

				if (m_registry->hasComponent<RigidBody2D>(e))
				{
					auto& rb = m_registry->getComponent<RigidBody2D>(e);
					rb.velocity = vec2(0.0f);
					m_simulation2D.setPosition(rb.simulationId, worldPos);
					m_simulation2D.setVelocity(rb.simulationId, vec2(0.0f));
					m_registry->setComponentDirty(rb);
				}

				if (m_registry->hasComponent<Shape>(e))
				{
					auto& s = m_registry->getComponent<Shape>(e);
					vec2 currentPos = getShapePosition(s, e);
					vec2 delta = worldPos - currentPos;
					offsetShapeParameters(s, delta);
					m_registry->setComponentDirty(s);
				}
			}

			m_isSceneDirty = true;
		}

		void resetClusterToInitialPosition(ClusterInstance& cluster)
		{
			auto& restData = getOrCreateClusterRestData(cluster);
			cluster.rotation = restData.initialRotation;
			resetClusterShapeAtPosition(cluster, restData.initialPosition);
		}

		void pickAtWorld(vec2 worldPos)
		{
			bool isCtrl = ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeyShift;
			bool pickIndividual = m_selectIndividualClusterEntities || ImGui::GetIO().KeyAlt;

			// 1. Check clusters (find closest cluster entity if any)
			float bestClusterDist = 1.0f;
			Entity bestClusterMember = INVALID_ENTITY;
			uint32_t bestClusterId = 0;

			for (const auto& cluster : m_services->clusters().getAll())
			{
				for (Entity member : cluster.entities)
				{
					float memberDist = 999.0f;
					if (m_registry->hasComponent<Shape>(member))
					{
						const auto& s = m_registry->getComponent<Shape>(member);
						const auto& sdfs = m_services->shapes().getSDFs();
						if (s.distanceFieldId < sdfs.size() && sdfs[s.distanceFieldId])
						{
							float params[12];
							packSdfParameters(params, s.parameters, m_services->time().time(), worldPos, 0.0f);
							float dist = sdfs[s.distanceFieldId]->getValue(params);
							if (dist <= 0.2f)
							{
								memberDist = std::max(0.0f, dist);
							}
						}
					}
					if (m_registry->hasComponent<Transform>(member))
					{
						vec2 memberPos(m_registry->getComponent<Transform>(member).position);
						float d = glm::distance(worldPos, memberPos);
						if (d < 1.0f && d < memberDist)
						{
							memberDist = d;
						}
					}

					if (memberDist < bestClusterDist)
					{
						bestClusterDist = memberDist;
						bestClusterMember = member;
						bestClusterId = cluster.id;
					}
				}
			}

			if (bestClusterMember != INVALID_ENTITY)
			{
				if (pickIndividual)
				{
					if (!isCtrl && isEntitySelected(bestClusterMember))
					{
						m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestClusterMember)};
						m_hasShiftAnchor = true;
						return; // Part of current selection, keep for dragging
					}
					selectEntity(bestClusterMember, isCtrl);
					m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestClusterMember)};
					m_hasShiftAnchor = true;
					return;
				}
				else
				{
					if (!isCtrl && isClusterSelected(bestClusterId))
					{
						m_shiftAnchorItem = {HierarchyItemType::Cluster, bestClusterId};
						m_hasShiftAnchor = true;
						return; // Part of current selection, keep for dragging
					}
					selectCluster(bestClusterId, isCtrl);
					m_shiftAnchorItem = {HierarchyItemType::Cluster, bestClusterId};
					m_hasShiftAnchor = true;
					return;
				}
			}

			// 2. Check standalone shapes using exact Signed Distance Field evaluation
			auto shapeArray = m_registry->getComponentArray<Shape>();
			if (shapeArray && shapeArray->getSize() > 0)
			{
				const auto& sdfs = m_services->shapes().getSDFs();
				float bestDist = 0.5f; // Boundary tolerance for clicking near outlines/lines
				Entity bestEntity = INVALID_ENTITY;

				// Iterate in reverse (topmost / last-drawn shapes first)
				for (int i = static_cast<int>(shapeArray->getSize()) - 1; i >= 0; --i)
				{
					Entity e = shapeArray->getEntityAtIdx(i);

					// Skip entities belonging to a cluster
					if (m_services->clusters().getClusterForEntity(e) != nullptr)
					{
						continue;
					}

					const auto& s = shapeArray->getDataAtIdx(i);
					if (s.distanceFieldId < sdfs.size() && sdfs[s.distanceFieldId])
					{
						float params[12];
						packSdfParameters(params, s.parameters, m_services->time().time(), worldPos, 0.0f);
						float dist = sdfs[s.distanceFieldId]->getValue(params);

						if (dist <= 0.0f)
						{
							// Direct click inside the shape! Topmost shape wins immediately.
							if (!isCtrl && isEntitySelected(e))
							{
								m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
								m_hasShiftAnchor = true;
								return; // Part of current selection, keep for dragging
							}
							selectEntity(e, isCtrl);
							m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(e)};
							m_hasShiftAnchor = true;
							return;
						}
						else if (dist < bestDist)
						{
							bestDist = dist;
							bestEntity = e;
						}
					}
					else if (m_registry->hasComponent<Transform>(e))
					{
						vec2 pos(m_registry->getComponent<Transform>(e).position);
						float dist = glm::distance(worldPos, pos);
						if (dist < 1.5f && dist < bestDist)
						{
							bestDist = dist;
							bestEntity = e;
						}
					}
				}

				if (bestEntity != INVALID_ENTITY)
				{
					if (!isCtrl && isEntitySelected(bestEntity))
					{
						m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
						m_hasShiftAnchor = true;
						return; // Part of current selection, keep for dragging
					}
					selectEntity(bestEntity, isCtrl);
					m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
					m_hasShiftAnchor = true;
					return;
				}
			}

			// 3. Fallback: check other entities with Transform (e.g. dots, camera markers)
			auto transformArray = m_registry->getComponentArray<Transform>();
			if (transformArray)
			{
				Entity camEntity = m_services->render().getCameraEntity();
				float bestDist = 1.0f;
				Entity bestEntity = INVALID_ENTITY;

				for (size_t i = 0; i < transformArray->getSize(); ++i)
				{
					Entity e = transformArray->getEntityAtIdx(i);
					if (e == camEntity || m_services->clusters().getClusterForEntity(e) != nullptr)
					{
						continue;
					}

					vec2 pos(transformArray->getDataAtIdx(i).position);
					float dist = glm::distance(worldPos, pos);
					if (dist < bestDist)
					{
						bestDist = dist;
						bestEntity = e;
					}
				}

				if (bestEntity != INVALID_ENTITY)
				{
					if (!isCtrl && isEntitySelected(bestEntity))
					{
						m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
						m_hasShiftAnchor = true;
						return; // Part of current selection, keep for dragging
					}
					selectEntity(bestEntity, isCtrl);
					m_shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(bestEntity)};
					m_hasShiftAnchor = true;
					return;
				}
			}

			// If clicked empty space, deselect (unless Ctrl is held)
			if (!isCtrl)
			{
				deselectAll();
			}
		}

		bool isEntitySelected(Entity e) const
		{
			return (m_selectedEntities.count(e) > 0) || (m_selectedEntity == e);
		}

		bool isClusterSelected(uint32_t cId) const
		{
			return (m_selectedClusterIds.count(cId) > 0) || (m_selectedClusterId == cId);
		}

		bool isGroupSelected(uint32_t gId) const
		{
			return m_selectedGroupId == gId;
		}

		void selectEntity(Entity e, bool multiSelect = false)
		{
			if (multiSelect)
			{
				m_selectedGroupId = 0;
				if (m_selectedEntities.count(e))
				{
					m_selectedEntities.erase(e);
					if (m_selectedEntity == e)
					{
						m_selectedEntity = m_selectedEntities.empty() ? INVALID_ENTITY : *m_selectedEntities.begin();
					}
				}
				else
				{
					m_selectedEntities.insert(e);
					m_selectedEntity = e;
				}
			}
			else
			{
				m_selectedClusterIds.clear();
				m_selectedClusterId = 0;
				m_selectedGroupId = 0;
				m_selectedEntities.clear();
				m_selectedEntities.insert(e);
				m_selectedEntity = e;
			}
		}

		void selectCluster(uint32_t cId, bool multiSelect = false)
		{
			if (multiSelect)
			{
				m_selectedGroupId = 0;
				if (m_selectedClusterIds.count(cId))
				{
					m_selectedClusterIds.erase(cId);
					if (m_selectedClusterId == cId)
					{
						m_selectedClusterId = m_selectedClusterIds.empty() ? 0 : *m_selectedClusterIds.begin();
					}
				}
				else
				{
					m_selectedClusterIds.insert(cId);
					m_selectedClusterId = cId;
					auto* c = m_services->clusters().getById(cId);
					if (c)
					{
						for (Entity member : c->entities)
						{
							m_selectedEntities.erase(member);
						}
					}
				}
			}
			else
			{
				m_selectedEntities.clear();
				m_selectedEntity = INVALID_ENTITY;
				m_selectedGroupId = 0;
				m_selectedClusterIds.clear();
				m_selectedClusterIds.insert(cId);
				m_selectedClusterId = cId;
			}
		}

		void selectGroup(uint32_t gId)
		{
			deselectAll();
			m_selectedGroupId = gId;
		}

		void selectItem(const HierarchyItem& item, bool multiSelect)
		{
			if (item.type == HierarchyItemType::Entity)
			{
				selectEntity(static_cast<Entity>(item.id), multiSelect);
			}
			else if (item.type == HierarchyItemType::Cluster)
			{
				selectCluster(item.id, multiSelect);
			}
			else if (item.type == HierarchyItemType::Group)
			{
				selectGroup(item.id);
			}
		}

		void handleHierarchyItemClick(const HierarchyItem& item)
		{
			bool isShift = ImGui::GetIO().KeyShift;
			bool isCtrl = ImGui::GetIO().KeyCtrl;

			if (isShift && m_hasShiftAnchor)
			{
				// Shift-click range selection!
				m_pendingShiftTarget = item;
				m_pendingShiftIsCtrl = isCtrl;
				m_hasPendingShift = true;
			}
			else if (isCtrl)
			{
				selectItem(item, true);
				m_shiftAnchorItem = item;
				m_hasShiftAnchor = true;
			}
			else
			{
				selectItem(item, false);
				m_shiftAnchorItem = item;
				m_hasShiftAnchor = true;
			}
		}

		void applyRangeSelection(const HierarchyItem& fromItem, const HierarchyItem& toItem, bool keepExistingSelection)
		{
			int idxA = -1;
			int idxB = -1;

			for (int i = 0; i < static_cast<int>(m_visibleHierarchyItems.size()); ++i)
			{
				if (m_visibleHierarchyItems[i] == fromItem)
					idxA = i;
				if (m_visibleHierarchyItems[i] == toItem)
					idxB = i;
			}

			if (idxA == -1 || idxB == -1)
			{
				selectItem(toItem, keepExistingSelection);
				m_shiftAnchorItem = toItem;
				m_hasShiftAnchor = true;
				return;
			}

			if (!keepExistingSelection)
			{
				deselectAll();
			}

			int startIdx = std::min(idxA, idxB);
			int endIdx = std::max(idxA, idxB);

			// Pre-gather any clusters in range to avoid double-selecting their member entities
			std::unordered_set<uint32_t> clustersInRange;
			for (int i = startIdx; i <= endIdx; ++i)
			{
				if (m_visibleHierarchyItems[i].type == HierarchyItemType::Cluster)
				{
					clustersInRange.insert(m_visibleHierarchyItems[i].id);
				}
			}

			for (int i = startIdx; i <= endIdx; ++i)
			{
				const auto& item = m_visibleHierarchyItems[i];
				if (item.type == HierarchyItemType::Entity)
				{
					Entity e = static_cast<Entity>(item.id);
					auto* c = m_services->clusters().getClusterForEntity(e);
					if (c && clustersInRange.count(c->id))
					{
						continue; // Parent cluster is already selected in range
					}
					m_selectedEntities.insert(e);
					m_selectedEntity = e;
				}
				else if (item.type == HierarchyItemType::Cluster)
				{
					m_selectedClusterIds.insert(item.id);
					m_selectedClusterId = item.id;
					auto* c = m_services->clusters().getById(item.id);
					if (c)
					{
						for (Entity member : c->entities)
						{
							m_selectedEntities.erase(member);
						}
					}
				}
				else if (item.type == HierarchyItemType::Group)
				{
					auto* g = findGroup(item.id);
					if (g)
					{
						for (Entity e : g->entities)
							m_selectedEntities.insert(e);
						for (uint32_t cId : g->clusterIds)
							m_selectedClusterIds.insert(cId);
					}
				}
			}

			// Keep m_shiftAnchorItem unchanged so subsequent Shift-clicks adjust the range!
			m_hasShiftAnchor = true;
		}

		EditorGroup* findGroup(uint32_t gId)
		{
			for (auto& g : m_groups)
			{
				if (g.id == gId)
					return &g;
			}
			return nullptr;
		}

		EditorGroup& createGroup(const std::string& name = "New Group")
		{
			EditorGroup g;
			g.id = m_nextGroupId++;
			g.name = name;
			m_groups.push_back(std::move(g));
			selectGroup(m_groups.back().id);
			m_isSceneDirty = true;
			return m_groups.back();
		}

		void groupSelected()
		{
			if (m_selectedEntities.empty() && m_selectedClusterIds.empty())
				return;

			EditorGroup group;
			group.id = m_nextGroupId++;
			group.name = "Group " + std::to_string(group.id);

			// Remove selected items from any existing groups
			for (auto& g : m_groups)
			{
				for (Entity e : m_selectedEntities)
				{
					auto it = std::find(g.entities.begin(), g.entities.end(), e);
					if (it != g.entities.end())
						g.entities.erase(it);
				}
				for (uint32_t cId : m_selectedClusterIds)
				{
					auto it = std::find(g.clusterIds.begin(), g.clusterIds.end(), cId);
					if (it != g.clusterIds.end())
						g.clusterIds.erase(it);
				}
			}

			for (Entity e : m_selectedEntities)
			{
				auto* c = m_services->clusters().getClusterForEntity(e);
				if (c)
				{
					if (std::find(group.clusterIds.begin(), group.clusterIds.end(), c->id) == group.clusterIds.end())
					{
						group.clusterIds.push_back(c->id);
					}
				}
				else
				{
					if (std::find(group.entities.begin(), group.entities.end(), e) == group.entities.end())
					{
						group.entities.push_back(e);
					}
				}
			}
			for (uint32_t cId : m_selectedClusterIds)
			{
				if (std::find(group.clusterIds.begin(), group.clusterIds.end(), cId) == group.clusterIds.end())
				{
					group.clusterIds.push_back(cId);
				}
			}

			m_groups.push_back(std::move(group));
			selectGroup(m_groups.back().id);
			m_isSceneDirty = true;
			Logger::log("[SceneEditor] Grouped selected items into: " + m_groups.back().name);
		}

		void ungroupSelected()
		{
			if (m_selectedGroupId != 0)
			{
				auto it = std::find_if(m_groups.begin(), m_groups.end(),
									   [this](const EditorGroup& g) { return g.id == m_selectedGroupId; });
				if (it != m_groups.end())
				{
					Logger::log("[SceneEditor] Ungrouped: " + it->name);
					m_groups.erase(it);
					m_selectedGroupId = 0;
					m_isSceneDirty = true;
				}
			}
		}

		bool canGroupSelected() const
		{
			return (m_selectedEntities.size() + m_selectedClusterIds.size() > 0);
		}

		bool isEntityInAnyGroup(Entity e) const
		{
			for (const auto& g : m_groups)
			{
				if (std::find(g.entities.begin(), g.entities.end(), e) != g.entities.end())
					return true;
			}
			return false;
		}

		bool isClusterInAnyGroup(uint32_t cId) const
		{
			for (const auto& g : m_groups)
			{
				if (std::find(g.clusterIds.begin(), g.clusterIds.end(), cId) != g.clusterIds.end())
					return true;
			}
			return false;
		}

		void addEntityToGroup(uint32_t gId, Entity e)
		{
			auto* c = m_services->clusters().getClusterForEntity(e);
			if (c)
			{
				addClusterToGroup(gId, c->id);
				return;
			}
			removeFromAllGroups(e);
			auto* target = findGroup(gId);
			if (target)
			{
				if (std::find(target->entities.begin(), target->entities.end(), e) == target->entities.end())
				{
					target->entities.push_back(e);
					m_isSceneDirty = true;
				}
			}
		}

		void addClusterToGroup(uint32_t gId, uint32_t cId)
		{
			removeClusterFromAllGroups(cId);
			auto* target = findGroup(gId);
			if (target)
			{
				if (std::find(target->clusterIds.begin(), target->clusterIds.end(), cId) == target->clusterIds.end())
				{
					target->clusterIds.push_back(cId);
					m_isSceneDirty = true;
				}
			}
		}

		void removeFromAllGroups(Entity e)
		{
			for (auto& g : m_groups)
			{
				auto it = std::find(g.entities.begin(), g.entities.end(), e);
				if (it != g.entities.end())
				{
					g.entities.erase(it);
					m_isSceneDirty = true;
				}
			}
		}

		void removeClusterFromAllGroups(uint32_t cId)
		{
			for (auto& g : m_groups)
			{
				auto it = std::find(g.clusterIds.begin(), g.clusterIds.end(), cId);
				if (it != g.clusterIds.end())
				{
					g.clusterIds.erase(it);
					m_isSceneDirty = true;
				}
			}
		}

		void cleanGroups()
		{
			for (auto& g : m_groups)
			{
				g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
												[this](Entity e) { return !m_registry->isEntityAlive(e); }),
								 g.entities.end());

				g.clusterIds.erase(std::remove_if(g.clusterIds.begin(), g.clusterIds.end(), [this](uint32_t cId)
												  { return m_services->clusters().getById(cId) == nullptr; }),
								   g.clusterIds.end());
			}
		}

		std::string formatEntityLabel(Entity e, const ClusterInstance* parentCluster = nullptr)
		{
			if (e == m_services->render().getCameraEntity())
			{
				return "Main Camera (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
			}

			if (parentCluster)
			{
				for (const auto& [name, ent] : parentCluster->localNames)
				{
					if (ent == e)
					{
						return name + " (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
					}
				}
			}

			std::string name = m_services->names().getName(e);
			if (!name.empty())
			{
				return name + " (Entity " + std::to_string(static_cast<uint32_t>(e)) + ")";
			}

			return "Entity " + std::to_string(static_cast<uint32_t>(e));
		}

		void syncConstraintClusterMembership(Entity constraintEnt, Entity entA, Entity entB)
		{
			if (entA == INVALID_ENTITY || entB == INVALID_ENTITY)
				return;

			auto* clusterA = m_services->clusters().getClusterForEntity(entA);
			auto* clusterB = m_services->clusters().getClusterForEntity(entB);
			auto* currentCluster = m_services->clusters().getClusterForEntity(constraintEnt);

			if (clusterA && clusterA == clusterB)
			{
				if (currentCluster != clusterA)
				{
					if (currentCluster)
					{
						auto it =
							std::find(currentCluster->entities.begin(), currentCluster->entities.end(), constraintEnt);
						if (it != currentCluster->entities.end())
							currentCluster->entities.erase(it);
					}
					if (std::find(clusterA->entities.begin(), clusterA->entities.end(), constraintEnt) ==
						clusterA->entities.end())
					{
						clusterA->entities.push_back(constraintEnt);
					}
				}
			}
			else if (currentCluster)
			{
				auto it = std::find(currentCluster->entities.begin(), currentCluster->entities.end(), constraintEnt);
				if (it != currentCluster->entities.end())
					currentCluster->entities.erase(it);
			}
		}

		void syncAllConstraintsClusterMembership()
		{
			auto springArray = m_registry->getComponentArray<Spring>();
			if (springArray)
			{
				for (size_t i = 0; i < springArray->getSize(); ++i)
				{
					Entity spEnt = springArray->getEntityAtIdx(i);
					auto& sp = springArray->getDataAtIdx(i);
					syncConstraintClusterMembership(spEnt, sp.entityA, sp.entityB);
				}
			}

			auto dcArray = m_registry->getComponentArray<DistanceConstraint>();
			if (dcArray)
			{
				for (size_t i = 0; i < dcArray->getSize(); ++i)
				{
					Entity dcEnt = dcArray->getEntityAtIdx(i);
					auto& dc = dcArray->getDataAtIdx(i);
					syncConstraintClusterMembership(dcEnt, dc.entityA, dc.entityB);
				}
			}
		}

		vec2 getEntityPosition(Entity e)
		{
			if (m_registry->hasComponent<Shape>(e))
			{
				return getShapePosition(m_registry->getComponent<Shape>(e), e);
			}
			if (m_registry->hasComponent<Transform>(e))
			{
				return vec2(m_registry->getComponent<Transform>(e).position);
			}
			return vec2(0.0f);
		}

		void setEntityPosition(Entity e, vec2 newPos)
		{
			vec2 currentPos = getEntityPosition(e);
			vec2 delta = newPos - currentPos;

			if (m_registry->hasComponent<Transform>(e))
			{
				auto& t = m_registry->getComponent<Transform>(e);
				t.position.x = newPos.x;
				t.position.y = newPos.y;
				m_registry->setComponentDirty(t);
				if (m_registry->hasComponent<RigidBody2D>(e))
				{
					auto& rb = m_registry->getComponent<RigidBody2D>(e);
					m_simulation2D.setPosition(rb.simulationId, newPos);
					m_simulation2D.setVelocity(rb.simulationId, vec2(0.0f));
					rb.velocity = vec2(0.0f);
					m_registry->setComponentDirty(rb);
				}
			}
			if (m_registry->hasComponent<Shape>(e))
			{
				auto& s = m_registry->getComponent<Shape>(e);
				offsetShapeParameters(s, delta);
				m_registry->setComponentDirty(s);
			}
		}

		void setClusterPosition(uint32_t cId, vec2 newPos)
		{
			auto* c = m_services->clusters().getById(cId);
			if (!c)
				return;

			vec2 delta = newPos - c->position;
			c->position = newPos;

			for (Entity e : c->entities)
			{
				if (m_registry->hasComponent<Transform>(e))
				{
					auto& t = m_registry->getComponent<Transform>(e);
					t.position.x += delta.x;
					t.position.y += delta.y;
					m_registry->setComponentDirty(t);
					if (m_registry->hasComponent<RigidBody2D>(e))
					{
						auto& rb = m_registry->getComponent<RigidBody2D>(e);
						m_simulation2D.setPosition(rb.simulationId, vec2(t.position));
						m_simulation2D.setVelocity(rb.simulationId, vec2(0.0f));
						rb.velocity = vec2(0.0f);
						m_registry->setComponentDirty(rb);
					}
				}
				if (m_registry->hasComponent<Shape>(e))
				{
					auto& s = m_registry->getComponent<Shape>(e);
					offsetShapeParameters(s, delta);
					m_registry->setComponentDirty(s);
				}
			}
		}

		bool hasSelection() const
		{
			return !m_selectedEntities.empty() || !m_selectedClusterIds.empty() || (m_selectedGroupId != 0) ||
				   (m_selectedEntity != INVALID_ENTITY) || (m_selectedClusterId != 0);
		}

		void deselectAll()
		{
			m_selectedEntities.clear();
			m_selectedClusterIds.clear();
			m_selectedEntity = INVALID_ENTITY;
			m_selectedClusterId = 0;
			m_selectedGroupId = 0;
			m_isDragging = false;
			m_dragEntityStartPositions.clear();
			m_dragClusterStartPositions.clear();
			m_hasShiftAnchor = false;
		}

		vec2 getSelectedPosition()
		{
			if (m_selectedGroupId != 0)
			{
				auto* g = findGroup(m_selectedGroupId);
				if (g && (!g->entities.empty() || !g->clusterIds.empty()))
				{
					if (!g->entities.empty())
						return getEntityPosition(g->entities[0]);
					if (!g->clusterIds.empty())
					{
						auto* c = m_services->clusters().getById(g->clusterIds[0]);
						if (c)
							return c->position;
					}
				}
			}
			if (m_selectedClusterId != 0)
			{
				auto* c = m_services->clusters().getById(m_selectedClusterId);
				if (c)
				{
					return c->position;
				}
			}
			if (m_selectedEntity != INVALID_ENTITY)
			{
				return getEntityPosition(m_selectedEntity);
			}
			return vec2(0.0f);
		}

		void setSelectedPosition(vec2 newPos)
		{
			if (m_selectedClusterId != 0)
			{
				setClusterPosition(m_selectedClusterId, newPos);
			}
			else if (m_selectedEntity != INVALID_ENTITY)
			{
				setEntityPosition(m_selectedEntity, newPos);
			}
		}

		void deleteSelected()
		{
			// Collect entities to delete
			std::unordered_set<Entity> entitiesToDelete = m_selectedEntities;
			if (m_selectedEntity != INVALID_ENTITY)
				entitiesToDelete.insert(m_selectedEntity);

			// Collect clusters to delete
			std::unordered_set<uint32_t> clustersToDelete = m_selectedClusterIds;
			if (m_selectedClusterId != 0)
				clustersToDelete.insert(m_selectedClusterId);

			if (m_selectedGroupId != 0 && entitiesToDelete.empty() && clustersToDelete.empty())
			{
				ungroupSelected();
				return;
			}

			for (uint32_t cId : clustersToDelete)
			{
				removeClusterFromAllGroups(cId);
				m_services->clusters().destroyCluster(cId);
			}

			// If any Spring or DistanceConstraint connects to an entity being deleted, destroy that constraint entity
			// too
			auto springArray = m_registry->getComponentArray<Spring>();
			if (springArray)
			{
				for (size_t i = 0; i < springArray->getSize(); ++i)
				{
					auto& sp = springArray->getDataAtIdx(i);
					if (entitiesToDelete.count(sp.entityA) || entitiesToDelete.count(sp.entityB))
					{
						Entity spEnt = springArray->getEntityAtIdx(i);
						if (!entitiesToDelete.count(spEnt))
						{
							m_services->names().removeName(spEnt);
							m_services->tags().removeAllTags(spEnt);
							m_registry->destroyEntity(spEnt);
						}
					}
				}
			}

			auto dcArray = m_registry->getComponentArray<DistanceConstraint>();
			if (dcArray)
			{
				for (size_t i = 0; i < dcArray->getSize(); ++i)
				{
					auto& dc = dcArray->getDataAtIdx(i);
					if (entitiesToDelete.count(dc.entityA) || entitiesToDelete.count(dc.entityB))
					{
						Entity dcEnt = dcArray->getEntityAtIdx(i);
						if (!entitiesToDelete.count(dcEnt))
						{
							m_services->names().removeName(dcEnt);
							m_services->tags().removeAllTags(dcEnt);
							m_registry->destroyEntity(dcEnt);
						}
					}
				}
			}

			Entity camEntity = m_services->render().getCameraEntity();
			for (Entity e : entitiesToDelete)
			{
				if (e == camEntity)
					continue; // Do not delete main camera

				removeFromAllGroups(e);

				auto* cluster = m_services->clusters().getClusterForEntity(e);
				if (cluster)
				{
					auto it = std::find(cluster->entities.begin(), cluster->entities.end(), e);
					if (it != cluster->entities.end())
					{
						cluster->entities.erase(it);
					}

					for (auto nit = cluster->localNames.begin(); nit != cluster->localNames.end();)
					{
						if (nit->second == e)
						{
							nit = cluster->localNames.erase(nit);
						}
						else
						{
							++nit;
						}
					}
				}
				m_services->names().removeName(e);
				m_services->tags().removeAllTags(e);
				m_registry->destroyEntity(e);
			}

			m_registry->freeRemovedComponents();
			m_2DWorldRenderContext.shapesNeedUpdate = true;
			deselectAll();
			m_isSceneDirty = true;
		}

		void duplicateSelected()
		{
			if (m_selectedEntity != INVALID_ENTITY && m_registry->hasComponent<Shape>(m_selectedEntity))
			{
				auto& origShape = m_registry->getComponent<Shape>(m_selectedEntity);
				auto& origTrans = m_registry->getComponent<Transform>(m_selectedEntity);

				Entity dup = m_registry->createEntity();
				auto& newTrans = m_registry->addComponent<Transform>(dup);
				newTrans = origTrans;
				newTrans.position.x += 1.0f;
				newTrans.position.y += 1.0f;

				auto& newShape = m_registry->addComponent<Shape>(dup);
				newShape = origShape;
				offsetShapeParameters(newShape, vec2(1.0f, 1.0f));
				m_registry->setComponentDirty(newShape);
				m_2DWorldRenderContext.shapesNeedUpdate = true;

				if (m_registry->hasComponent<RigidBody2D>(m_selectedEntity))
				{
					auto& origRb = m_registry->getComponent<RigidBody2D>(m_selectedEntity);
					auto& newRb = m_registry->addComponent<RigidBody2D>(dup);
					newRb.type = origRb.type;
					newRb.mass = origRb.mass;
					newRb.enableCollision = origRb.enableCollision;
					newRb.enableShapeCollision = origRb.enableShapeCollision;
				}

				m_selectedEntity = dup;
				m_isSceneDirty = true;
			}
		}

		// =====================================================================
		// Scene Operations
		// =====================================================================
		void newScene()
		{
			Entity camEntity = m_services->render().getCameraEntity();

			// 1. Destroy all non-camera entities
			for (Entity e = 0; e < m_registry->getEntityCount(); ++e)
			{
				if (e != camEntity && !m_registry->isEntityDestroyed(e))
				{
					m_registry->destroyEntity(e);
				}
			}

			// 2. Clear all names and tags
			m_services->names().clear();
			m_services->tags().clear();

			// 3. Free removed component arrays and entities immediately
			m_registry->freeRemovedComponents();

			// 4. Reset physics bodies, particles, and shapes
			m_simulation2D.resetBodiesAndShapes();
			m_simulation2D.resume();
			m_simulation2D.setGravity(0.0f);
			m_simulation2D.setDamping(1.0f);
			m_isSimulating = false;
			m_hasSnapshot = false;

			// 5. Clear clusters
			m_services->clusters().clear();
			m_clusterRestData.clear();
			m_clusterAssetWriteTimes.clear();

			// 6. Reset materials
			for (size_t i = 0; i < 16; ++i)
			{
				auto& mat = m_services->materials2D().get(static_cast<uint16_t>(i));
				mat.id = static_cast<uint16_t>(i);
				mat.name = (i == 0) ? "default" : ("mat_" + std::to_string(i));
				mat.color = (i < ColorPalette::Default.size()) ? vec4(ColorPalette::Default[i], 1.0f) : vec4(1.0f);
				mat.secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
				mat.pattern = Pattern2D::None;
				mat.patternScale = 1.0f;
			}

			// 7. Update render contexts
			m_2DWorldRenderContext.shapesNeedUpdate = true;
			m_UIRenderContext.shapesNeedUpdate = true;

			// 8. Reset editor selection and state
			m_selectedEntity = INVALID_ENTITY;
			m_selectedClusterId = 0;
			m_selectedGroupId = 0;
			m_selectedEntities.clear();
			m_selectedClusterIds.clear();
			m_groups.clear();
			m_nextGroupId = 1;
			m_isDragging = false;
			m_dragEntityStartPositions.clear();
			m_dragClusterStartPositions.clear();
			m_currentScenePath.clear();
			m_isSceneDirty = false;
			Logger::log("[SceneEditor] Cleared scene.");
		}

		void openLoadSceneDialog()
		{
			static SDL_DialogFileFilter filters[1] = {{"Weird Scene (*.weirdscene)", "weirdscene"}};
			std::string defaultPath = m_project.isOpen() ? m_project.getAssetsPath().string() : "";
			SDL_ShowOpenFileDialog(onOpenSceneCallback, this, nullptr, filters, 1,
								   defaultPath.empty() ? nullptr : defaultPath.c_str(), false);
		}

		void saveScenePrompt()
		{
			if (!m_currentScenePath.empty())
			{
				saveScene(m_currentScenePath);
			}
			else
			{
				openSaveSceneDialog();
			}
		}

		void openSaveSceneDialog()
		{
			static SDL_DialogFileFilter filters[1] = {{"Weird Scene (*.weirdscene)", "weirdscene"}};
			std::string defaultPath = m_project.isOpen() ? m_project.getAssetsPath().string() : "";
			SDL_ShowSaveFileDialog(onSaveSceneCallback, this, nullptr, filters, 1, "scene.weirdscene");
		}

		void loadScene(const std::string& path)
		{
			newScene();
			SceneSerializer::load(*this, path);
			syncAllConstraintsClusterMembership();
			m_currentScenePath = path;
			m_isSceneDirty = false;

			// Check if any cluster instance's asset file on disk has been modified since the scene was saved
			std::error_code ec;
			auto sceneWriteTime = std::filesystem::last_write_time(path, ec);
			std::vector<uint32_t> clustersToAutoReload;
			for (const auto& c : m_services->clusters().getAll())
			{
				if (!c.assetPath.empty())
				{
					std::filesystem::path resolved = resolveClusterPath(c.assetPath);
					if (std::filesystem::exists(resolved, ec))
					{
						auto assetWriteTime = std::filesystem::last_write_time(resolved, ec);
						std::string normPath = ShapeService::normalizePath(resolved.string());
						m_clusterAssetWriteTimes[normPath] = assetWriteTime;
						if (!ec && (assetWriteTime > sceneWriteTime))
						{
							clustersToAutoReload.push_back(c.id);
						}
					}
				}
			}
			for (uint32_t cId : clustersToAutoReload)
			{
				reloadCluster(cId);
			}

			// Restore editor groups from scene file
			try
			{
				std::ifstream inFile(path);
				if (inFile.is_open())
				{
					nlohmann::json j;
					inFile >> j;
					inFile.close();

					if (j.contains("editorGroups") && j["editorGroups"].is_array())
					{
						for (const auto& gj : j["editorGroups"])
						{
							EditorGroup g;
							g.id = gj.value("id", m_nextGroupId);
							g.name = gj.value("name", "Group " + std::to_string(g.id));
							if (gj.contains("entities") && gj["entities"].is_array())
							{
								for (const auto& ej : gj["entities"])
								{
									Entity e = static_cast<Entity>(ej.get<uint32_t>());
									if (m_registry->isEntityAlive(e))
									{
										g.entities.push_back(e);
									}
								}
							}
							if (gj.contains("clusterIds") && gj["clusterIds"].is_array())
							{
								for (const auto& cj : gj["clusterIds"])
								{
									uint32_t cId = cj.get<uint32_t>();
									if (m_services->clusters().getById(cId))
									{
										g.clusterIds.push_back(cId);
									}
								}
							}
							m_nextGroupId = std::max(m_nextGroupId, g.id + 1);
							m_groups.push_back(std::move(g));
						}
					}
				}
			}
			catch (const std::exception& ex)
			{
				Logger::log("[SceneEditor] Error loading editor groups: " + std::string(ex.what()));
			}

			Logger::log("[SceneEditor] Loaded scene: " + path);
		}

		void saveScene(const std::string& path)
		{
			std::string finalPath = path;
			if (!finalPath.ends_with(".weirdscene"))
			{
				finalPath += ".weirdscene";
			}

			m_services->serialization().saveScene(finalPath);
			m_currentScenePath = finalPath;
			m_isSceneDirty = false;
			m_project.scanAssets();

			// Persist editor groups in scene file
			try
			{
				std::ifstream inFile(finalPath);
				if (inFile.is_open())
				{
					nlohmann::json j;
					inFile >> j;
					inFile.close();

					nlohmann::json groupsJson = nlohmann::json::array();
					for (const auto& g : m_groups)
					{
						nlohmann::json gj;
						gj["id"] = g.id;
						gj["name"] = g.name;
						gj["entities"] = nlohmann::json::array();
						for (Entity e : g.entities)
						{
							gj["entities"].push_back(static_cast<uint32_t>(e));
						}
						gj["clusterIds"] = g.clusterIds;
						groupsJson.push_back(gj);
					}
					j["editorGroups"] = groupsJson;

					std::ofstream outFile(finalPath);
					if (outFile.is_open())
					{
						outFile << j.dump(4);
						outFile.close();
					}
				}
			}
			catch (const std::exception& ex)
			{
				Logger::log("[SceneEditor] Error saving editor groups: " + std::string(ex.what()));
			}

			Logger::log("[SceneEditor] Saved scene to " + finalPath);
		}

		void pollFileActions()
		{
			std::string loadPath;
			std::string savePath;
			{
				std::lock_guard<std::mutex> lock(m_fileMutex);
				loadPath = std::move(m_pendingLoadScenePath);
				savePath = std::move(m_pendingSaveScenePath);
				m_pendingLoadScenePath.clear();
				m_pendingSaveScenePath.clear();
			}

			if (!loadPath.empty())
			{
				loadScene(loadPath);
			}
			if (!savePath.empty())
			{
				saveScene(savePath);
			}
		}

		static void SDLCALL onOpenSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
		{
			auto* self = static_cast<SceneEditor*>(userdata);
			if (self && filelist && *filelist)
			{
				std::lock_guard<std::mutex> lock(self->m_fileMutex);
				self->m_pendingLoadScenePath = *filelist;
			}
		}

		static void SDLCALL onSaveSceneCallback(void* userdata, const char* const* filelist, int /*filter*/)
		{
			auto* self = static_cast<SceneEditor*>(userdata);
			if (self && filelist && *filelist)
			{
				std::lock_guard<std::mutex> lock(self->m_fileMutex);
				self->m_pendingSaveScenePath = *filelist;
			}
		}

		void toggleSimulation()
		{
			m_isSimulating = !m_isSimulating;
			if (m_isSimulating)
			{
				// Take temporary snapshot before physics simulation
				m_services->serialization().saveSnapshot("temp_editor.weirdsnap");
				m_hasSnapshot = true;
				m_simulation2D.setGravity(m_playGravity);
				m_simulation2D.setDamping(m_playDamping);
				Logger::log("[SceneEditor] Simulation started (play mode).");
			}
			else
			{
				m_simulation2D.setGravity(0.0f);
				m_simulation2D.setDamping(1.0f);
				// Revert to snapshot on pause
				if (m_hasSnapshot)
				{
					m_services->serialization().loadSnapshot("temp_editor.weirdsnap");
					Logger::log("[SceneEditor] Simulation paused (design mode, reverted to snapshot).");
				}
			}
		}

		// =====================================================================
		// Spawning Helpers
		// =====================================================================
		Entity spawnDefaultShape(uint16_t shapeType, vec2 pos)
		{
			Entity e = m_registry->createEntity();
			auto& t = m_registry->addComponent<Transform>(e);
			t.position = vec3(pos.x, pos.y, 0.0f);

			auto& s = m_registry->addComponent<Shape>(e);
			s.distanceFieldId = shapeType;
			s.combination = CombinationType::Addition;
			s.hasCollisions = true;
			s.material = 1;
			s.smoothFactor = 1.0f;

			// Defaults for common shapes
			if (shapeType == DefaultShapes::Circle)
			{
				s.parameters[DefaultShapes::Circle::PosX] = pos.x;
				s.parameters[DefaultShapes::Circle::PosY] = pos.y;
				s.parameters[DefaultShapes::Circle::Radius] = 1.5f;
			}
			else if (shapeType == DefaultShapes::Box)
			{
				s.parameters[DefaultShapes::Box::PosX] = pos.x;
				s.parameters[DefaultShapes::Box::PosY] = pos.y;
				s.parameters[DefaultShapes::Box::SizeX] = 2.0f;
				s.parameters[DefaultShapes::Box::SizeY] = 1.0f;
			}
			else if (shapeType == DefaultShapes::RoundBox)
			{
				s.parameters[DefaultShapes::RoundBox::PosX] = pos.x;
				s.parameters[DefaultShapes::RoundBox::PosY] = pos.y;
				s.parameters[DefaultShapes::RoundBox::SizeX] = 2.0f;
				s.parameters[DefaultShapes::RoundBox::SizeY] = 1.0f;
				s.parameters[DefaultShapes::RoundBox::Radius] = 0.3f;
			}
			else if (shapeType == DefaultShapes::Triangle)
			{
				s.parameters[DefaultShapes::Triangle::PosX] = pos.x;
				s.parameters[DefaultShapes::Triangle::PosY] = pos.y;
				s.parameters[DefaultShapes::Triangle::Width] = 2.0f;
				s.parameters[DefaultShapes::Triangle::Height] = 2.0f;
			}
			else if (shapeType == DefaultShapes::Line)
			{
				s.parameters[DefaultShapes::Line::StartX] = pos.x - 1.5f;
				s.parameters[DefaultShapes::Line::StartY] = pos.y;
				s.parameters[DefaultShapes::Line::EndX] = pos.x + 1.5f;
				s.parameters[DefaultShapes::Line::EndY] = pos.y;
				s.parameters[DefaultShapes::Line::Width] = 0.15f;
			}
			else if (shapeType == DefaultShapes::Ramp)
			{
				s.parameters[DefaultShapes::Ramp::PosX] = pos.x;
				s.parameters[DefaultShapes::Ramp::PosY] = pos.y;
				s.parameters[DefaultShapes::Ramp::Width] = 3.0f;
				s.parameters[DefaultShapes::Ramp::Height] = 1.5f;
				s.parameters[DefaultShapes::Ramp::Skew] = 0.5f;
			}
			else if (shapeType == DefaultShapes::Star)
			{
				s.parameters[0] = pos.x;
				s.parameters[1] = pos.y;
				s.parameters[2] = 1.5f;
				s.parameters[3] = 0.5f;
				s.parameters[4] = 5.0f;
				s.parameters[5] = 0.0f;
			}
			else
			{
				s.parameters[0] = pos.x;
				s.parameters[1] = pos.y;
			}

			m_registry->setComponentDirty(s);
			m_2DWorldRenderContext.shapesNeedUpdate = true;

			m_selectedEntity = e;
			m_selectedClusterId = 0;
			m_isSceneDirty = true;
			return e;
		}

		Entity spawnCustomShape(const std::string& assetPath, vec2 pos)
		{
			ShapeId id = m_services->shapes().loadCustomShape(assetPath);
			Entity e = m_registry->createEntity();
			auto& t = m_registry->addComponent<Transform>(e);
			t.position = vec3(pos.x, pos.y, 0.0f);

			auto& s = m_registry->addComponent<Shape>(e);
			s.distanceFieldId = id;
			s.combination = CombinationType::Addition;
			s.hasCollisions = true;
			s.material = 2;
			s.smoothFactor = 1.0f;
			if (const auto* customData = m_services->shapes().getCustomShapeData(id))
			{
				std::copy(customData->defaultParameters.begin(), customData->defaultParameters.end(), s.parameters);
			}

			if (pos.x != 0.0f || pos.y != 0.0f)
			{
				offsetShapeParameters(s, pos);
			}

			m_registry->setComponentDirty(s);
			m_2DWorldRenderContext.shapesNeedUpdate = true;

			std::filesystem::path p(assetPath);
			std::string baseName = p.stem().string();
			if (!baseName.empty())
			{
				std::string name = baseName + "_" + std::to_string(static_cast<uint32_t>(e));
				m_services->names().setName(e, name);
			}

			m_selectedEntity = e;
			m_selectedClusterId = 0;
			m_isSceneDirty = true;
			return e;
		}

		void spawnCluster(const std::string& assetPath, vec2 pos)
		{
			auto& cluster = m_services->clusters().instantiate(assetPath, pos);
			syncAllConstraintsClusterMembership();
			m_selectedClusterId = cluster.id;
			m_selectedEntity = INVALID_ENTITY;
			m_isSceneDirty = true;

			std::filesystem::path resolved = resolveClusterPath(assetPath);
			std::error_code ec;
			if (std::filesystem::exists(resolved, ec))
			{
				auto writeTime = std::filesystem::last_write_time(resolved, ec);
				if (!ec)
				{
					m_clusterAssetWriteTimes[ShapeService::normalizePath(resolved.string())] = writeTime;
				}
			}
		}

		vec2 getCameraCenterWorld()
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			return vec2(cam.position.x, cam.position.y);
		}

		// =====================================================================
		// Tool Launching & Asset Creation
		// =====================================================================
		std::filesystem::path getExecutableDirectory()
		{
#if defined(_WIN32)
			char buffer[MAX_PATH];
			DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
			if (length > 0)
			{
				return std::filesystem::path(buffer).parent_path();
			}
#endif
			return std::filesystem::current_path();
		}

		std::filesystem::path findToolExecutable(const std::string& toolBaseName)
		{
			auto exeDir = getExecutableDirectory();
			std::string exeName = toolBaseName;
#if defined(_WIN32)
			if (!exeName.ends_with(".exe"))
			{
				exeName += ".exe";
			}
#endif

			// 1. Same directory as WeirdEditor (when CMAKE_RUNTIME_OUTPUT_DIRECTORY is used)
			std::filesystem::path candidate = exeDir / exeName;
			if (std::filesystem::exists(candidate))
			{
				return std::filesystem::canonical(candidate);
			}

			// 2. Relative paths across build directories
			std::vector<std::filesystem::path> searchPaths = {
				exeDir / ".." / exeName,
				exeDir / ".." / "tools" / "bin" / exeName,
				exeDir / ".." / ".." / "tools" / "bin" / exeName,
				exeDir / ".." / ".." / "tools" / toolBaseName / exeName,
				exeDir / ".." / ".." / "tools" / toolBaseName / "RelWithDebInfo" / exeName,
				exeDir / ".." / ".." / "tools" / toolBaseName / "Debug" / exeName,
				exeDir / ".." / ".." / "tools" / toolBaseName / "Release" / exeName,
				std::filesystem::current_path() / "build" / "tools" / "bin" / exeName,
				std::filesystem::current_path() / "build" / "tools" / "bin" / "RelWithDebInfo" / exeName,
				std::filesystem::current_path() / "build" / "tools" / "bin" / "Debug" / exeName,
			};

			for (const auto& p : searchPaths)
			{
				if (std::filesystem::exists(p))
				{
					return std::filesystem::canonical(p);
				}
			}

			return "";
		}

		void launchTool(const std::string& toolBaseName, const std::string& filePath = "")
		{
			std::filesystem::path exePath = findToolExecutable(toolBaseName);
			if (exePath.empty())
			{
				Logger::error("[SceneEditor] Could not locate executable for: " + toolBaseName);
				return;
			}

			std::string cmd = "\"" + exePath.string() + "\"";
			if (!filePath.empty())
			{
				std::filesystem::path absPath = std::filesystem::absolute(filePath);
				cmd += " \"" + absPath.string() + "\"";
			}

			Logger::log("[SceneEditor] Launching tool: " + cmd);

#if defined(_WIN32)
			STARTUPINFOA si;
			PROCESS_INFORMATION pi;
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
			ZeroMemory(&pi, sizeof(pi));

			std::vector<char> cmdVec(cmd.begin(), cmd.end());
			cmdVec.push_back('\0');

			std::string workDir = exePath.parent_path().string();

			if (CreateProcessA(NULL, cmdVec.data(), NULL, NULL, FALSE, 0, NULL, workDir.c_str(), &si, &pi))
			{
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			else
			{
				Logger::error("[SceneEditor] Failed to launch process: " + std::to_string(GetLastError()));
			}
#else
			cmd += " &";
			std::system(cmd.c_str());
#endif
		}

		void launchClusterEditor(const std::string& filePath = "")
		{
			launchTool("WeirdClusterEditor", filePath);
		}

		void launchShapeEditor(const std::string& filePath = "")
		{
			launchTool("WeirdShapeEditor", filePath);
		}

		bool reloadCustomShape(const std::string& assetPath)
		{
			if (!m_services)
			{
				return false;
			}
			if (m_services->shapes().reloadCustomShape(assetPath))
			{
				const auto* customData = m_services->shapes().getCustomShapeData(assetPath);
				if (customData)
				{
					notifyShapeReloaded(customData->id);
				}
				else
				{
					m_2DWorldRenderContext.shapesNeedUpdate = true;
					m_UIRenderContext.shapesNeedUpdate = true;
				}
				return true;
			}
			return false;
		}

		void notifyShapeReloaded(ShapeId id)
		{
			if (!m_registry)
			{
				return;
			}

			bool anyEntityUpdated = false;
			for (Entity e = 0; e < m_registry->getEntityCount(); ++e)
			{
				if (m_registry->isEntityValid(e) && m_registry->hasComponent<Shape>(e))
				{
					auto& s = m_registry->getComponent<Shape>(e);
					if (s.distanceFieldId == id)
					{
						m_registry->setComponentDirty(s);
						anyEntityUpdated = true;
					}
				}
				if (m_registry->isEntityValid(e) && m_registry->hasComponent<UIShape>(e))
				{
					auto& s = m_registry->getComponent<UIShape>(e);
					if (s.distanceFieldId == id)
					{
						m_registry->setComponentDirty(s);
						anyEntityUpdated = true;
					}
				}
			}

			m_2DWorldRenderContext.shapesNeedUpdate = true;
			m_UIRenderContext.shapesNeedUpdate = true;
			m_isSceneDirty = true;

			if (m_services)
			{
				if (const auto* customData = m_services->shapes().getCustomShapeData(id))
				{
					Logger::log("[SceneEditor] Hot-reloaded custom shape: " + customData->filePath);
				}
			}
		}

		bool reloadCluster(uint32_t clusterId)
		{
			if (!m_services)
			{
				return false;
			}
			auto* cluster = m_services->clusters().getById(clusterId);
			if (!cluster || cluster->assetPath.empty())
			{
				return false;
			}

			std::filesystem::path resolved = resolveClusterPath(cluster->assetPath);
			if (!std::filesystem::exists(resolved))
			{
				Logger::error("[SceneEditor] Cannot reload cluster " + cluster->name + ": asset file not found at " +
							  cluster->assetPath);
				return false;
			}

			// Clear selection if it targeted this cluster's old entities
			for (Entity e : cluster->entities)
			{
				if (m_selectedEntity == e)
				{
					m_selectedEntity = INVALID_ENTITY;
				}
				m_selectedEntities.erase(e);
			}

			std::string resolvedStr = resolved.string();
			if (!m_services->clusters().reloadInstance(clusterId, resolvedStr))
			{
				return false;
			}

			// Clear cached rest data so it's recomputed fresh from the updated file
			m_clusterRestData.erase(clusterId);
			syncAllConstraintsClusterMembership();

			std::error_code ec;
			auto writeTime = std::filesystem::last_write_time(resolved, ec);
			if (!ec)
			{
				m_clusterAssetWriteTimes[ShapeService::normalizePath(resolvedStr)] = writeTime;
			}

			m_2DWorldRenderContext.shapesNeedUpdate = true;
			m_isSceneDirty = true;
			Logger::log("[SceneEditor] Reloaded cluster " + cluster->name + " from " + resolvedStr);
			return true;
		}

		int reloadClustersFromAsset(const std::string& assetPath)
		{
			if (!m_services)
			{
				return 0;
			}
			std::filesystem::path resolved = resolveClusterPath(assetPath);
			std::filesystem::path resolvedP =
				std::filesystem::exists(resolved) ? resolved : std::filesystem::path(assetPath);
			std::string targetFilename = resolvedP.filename().string();
			std::string targetStem = resolvedP.stem().string();

			std::vector<uint32_t> toReload;
			for (const auto& c : m_services->clusters().getAll())
			{
				std::filesystem::path cp(c.assetPath);
				if (cp == resolvedP || cp.filename().string() == targetFilename || c.type == targetStem)
				{
					toReload.push_back(c.id);
				}
			}

			int count = 0;
			for (uint32_t id : toReload)
			{
				if (reloadCluster(id))
				{
					count++;
				}
			}

			std::error_code ec;
			if (std::filesystem::exists(resolvedP, ec))
			{
				auto writeTime = std::filesystem::last_write_time(resolvedP, ec);
				if (!ec)
				{
					m_clusterAssetWriteTimes[ShapeService::normalizePath(resolvedP.string())] = writeTime;
				}
			}

			return count;
		}

		int reloadAllClusters()
		{
			if (!m_services)
			{
				return 0;
			}
			std::vector<uint32_t> allIds;
			for (const auto& c : m_services->clusters().getAll())
			{
				if (!c.assetPath.empty())
				{
					allIds.push_back(c.id);
				}
			}
			int count = 0;
			for (uint32_t id : allIds)
			{
				if (reloadCluster(id))
				{
					count++;
				}
			}
			Logger::log("[SceneEditor] Reloaded " + std::to_string(count) + " cluster(s) from asset files.");
			return count;
		}

		void pollAssetModifications()
		{
			if (!m_services)
			{
				return;
			}

			// 1. Poll custom shape modifications (.weirdshape)
			std::vector<ShapeId> reloadedIds;
			if (m_services->shapes().pollCustomShapesChanged(&reloadedIds))
			{
				for (ShapeId id : reloadedIds)
				{
					notifyShapeReloaded(id);
				}
			}

			// 2. Poll cluster modifications (.weirdcluster)
			std::unordered_set<std::string> modifiedClusterAssets;
			for (const auto& c : m_services->clusters().getAll())
			{
				if (c.assetPath.empty())
				{
					continue;
				}
				std::filesystem::path resolved = resolveClusterPath(c.assetPath);
				std::error_code ec;
				if (!std::filesystem::exists(resolved, ec) || ec)
				{
					continue;
				}
				auto writeTime = std::filesystem::last_write_time(resolved, ec);
				if (ec)
				{
					continue;
				}
				std::string normPath = ShapeService::normalizePath(resolved.string());
				auto it = m_clusterAssetWriteTimes.find(normPath);
				if (it == m_clusterAssetWriteTimes.end())
				{
					m_clusterAssetWriteTimes[normPath] = writeTime;
				}
				else if (writeTime > it->second)
				{
					it->second = writeTime;
					modifiedClusterAssets.insert(resolved.string());
				}
			}

			for (const auto& assetPath : modifiedClusterAssets)
			{
				reloadClustersFromAsset(assetPath);
			}
		}

		std::filesystem::path createNewClusterAsset(std::string name)
		{
			if (name.empty())
			{
				name = "NewCluster";
			}

			if (name.ends_with(".weirdcluster"))
			{
				name = name.substr(0, name.size() - 13);
			}

			std::filesystem::path targetDir =
				m_project.isOpen() ? m_project.getAssetsPath() : std::filesystem::path("examples/sample-scenes/assets");

			std::error_code ec;
			std::filesystem::create_directories(targetDir, ec);

			std::string finalName = name + ".weirdcluster";
			std::filesystem::path finalPath = targetDir / finalName;
			int counter = 1;
			while (std::filesystem::exists(finalPath))
			{
				finalName = name + "_" + std::to_string(counter++) + ".weirdcluster";
				finalPath = targetDir / finalName;
			}

			nlohmann::json j;
			j["version"] = 1;
			j["camera"] = {
				{"position", {0.0f, 0.0f, 25.0f}}, {"rotation", {0.0f, 0.0f, -1.0f}}, {"scale", {1.0f, 1.0f, 1.0f}}};
			j["entities"] = nlohmann::json::array();
			j["physics"] = {{"distanceConstraints", nlohmann::json::array()}, {"springs", nlohmann::json::array()}};

			std::ofstream outFile(finalPath);
			if (outFile.is_open())
			{
				outFile << j.dump(2);
				outFile.close();
				Logger::log("[SceneEditor] Created new cluster asset: " + finalPath.string());
				m_project.scanAssets();
				launchClusterEditor(finalPath.string());
				return finalPath;
			}
			else
			{
				Logger::error("[SceneEditor] Failed to create cluster asset at: " + finalPath.string());
				return "";
			}
		}

		std::filesystem::path createNewShapeAsset(std::string name)
		{
			if (name.empty())
			{
				name = "NewShape";
			}

			if (name.ends_with(".weirdshape"))
			{
				name = name.substr(0, name.size() - 11);
			}

			std::filesystem::path targetDir =
				m_project.isOpen() ? m_project.getAssetsPath() : std::filesystem::path("examples/sample-scenes/assets");

			std::error_code ec;
			std::filesystem::create_directories(targetDir, ec);

			std::string finalName = name + ".weirdshape";
			std::filesystem::path finalPath = targetDir / finalName;
			int counter = 1;
			while (std::filesystem::exists(finalPath))
			{
				finalName = name + "_" + std::to_string(counter++) + ".weirdshape";
				finalPath = targetDir / finalName;
			}

			NodeGraph graph;
			NodeGraphPresets::loadCircle(graph);
			nlohmann::json j = NodeGraphSerializer::serialize(graph);

			std::ofstream outFile(finalPath);
			if (outFile.is_open())
			{
				outFile << j.dump(2);
				outFile.close();
				Logger::log("[SceneEditor] Created new shape asset: " + finalPath.string());
				m_project.scanAssets();
				launchShapeEditor(finalPath.string());
				return finalPath;
			}
			else
			{
				Logger::error("[SceneEditor] Failed to create shape asset at: " + finalPath.string());
				return "";
			}
		}

		// =====================================================================
		// UI: Menu Bar
		// =====================================================================
		void renderMenuBar()
		{
			if (ImGui::BeginMainMenuBar())
			{
				m_menuBarHeight = ImGui::GetWindowSize().y;

				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::MenuItem("Open Project Folder..."))
					{
						m_project.openFolderDialog();
					}

					if (ImGui::BeginMenu("Recent Projects"))
					{
						if (m_project.getRecentProjects().empty())
						{
							ImGui::MenuItem("No Recent Projects", nullptr, false, false);
						}
						else
						{
							for (const auto& path : m_project.getRecentProjects())
							{
								if (ImGui::MenuItem(path.c_str()))
								{
									m_project.open(path);
								}
							}
						}
						ImGui::EndMenu();
					}

					ImGui::Separator();

					if (ImGui::MenuItem("New Scene", "Ctrl+N"))
					{
						newScene();
					}
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O"))
					{
						openLoadSceneDialog();
					}
					if (ImGui::MenuItem("Save Scene", "Ctrl+S"))
					{
						saveScenePrompt();
					}
					if (ImGui::MenuItem("Save Scene As..."))
					{
						openSaveSceneDialog();
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
					if (ImGui::MenuItem("Delete Selected", "Del / X", false, hasSelection()))
					{
						deleteSelected();
					}
					if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selectedEntity != INVALID_ENTITY))
					{
						duplicateSelected();
					}
					if (ImGui::MenuItem("Deselect", "Esc", false, hasSelection()))
					{
						deselectAll();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Group Selected", "Ctrl+G", false, canGroupSelected()))
					{
						groupSelected();
					}
					if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G", false, m_selectedGroupId != 0))
					{
						ungroupSelected();
					}
					if (ImGui::MenuItem("New Empty Group", nullptr))
					{
						createGroup();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Move Individual Rigidbodies", "C", &m_selectIndividualClusterEntities))
					{
					}
					if (ImGui::MenuItem("Reset Cluster to Initial Position", "R", false, m_selectedClusterId != 0))
					{
						auto* c = m_services->clusters().getById(m_selectedClusterId);
						if (c)
						{
							resetClusterToInitialPosition(*c);
						}
					}
					if (ImGui::MenuItem("Reload Selected Cluster (from file)", nullptr, false,
										m_selectedClusterId != 0))
					{
						reloadCluster(m_selectedClusterId);
					}
					if (ImGui::MenuItem("Reload All Clusters from Files", nullptr))
					{
						reloadAllClusters();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					if (ImGui::MenuItem("Reset Camera", "F"))
					{
						resetCamera();
					}
					ImGui::MenuItem("Show Grid", nullptr, &m_gridEnabled);
					ImGui::MenuItem("Snap to Grid", nullptr, &m_gridSnap);
					ImGui::SliderFloat("Grid Size", &m_gridSize, 0.25f, 5.0f, "%.2f");
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Simulation"))
				{
					if (ImGui::MenuItem(m_isSimulating ? "Stop Simulation" : "Play Simulation", "Space"))
					{
						toggleSimulation();
					}
					ImGui::Separator();
					ImGui::DragFloat("Play Gravity", &m_playGravity, 0.1f, -50.0f, 50.0f, "%.1f");
					ImGui::DragFloat("Play Damping", &m_playDamping, 0.001f, 0.0f, 1.0f, "%.3f");
					if (ImGui::MenuItem("Take Snapshot"))
					{
						m_services->serialization().saveSnapshot("dev.weirdsnap");
						m_hasSnapshot = true;
					}
					if (ImGui::MenuItem("Restore Snapshot", nullptr, false, m_hasSnapshot))
					{
						m_services->serialization().loadSnapshot("dev.weirdsnap");
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Tools"))
				{
					if (ImGui::MenuItem("Reload All Clusters from Files"))
					{
						reloadAllClusters();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Launch Cluster Editor"))
					{
						launchClusterEditor();
					}
					if (ImGui::MenuItem("Launch Shape Editor"))
					{
						launchShapeEditor();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("+ New Cluster in Assets..."))
					{
						m_openNewClusterModal = true;
					}
					if (ImGui::MenuItem("+ New Shape in Assets..."))
					{
						m_openNewShapeModal = true;
					}
					ImGui::EndMenu();
				}

				// Project status banner on the right of the menu bar
				std::string statusText;
				if (m_project.isOpen())
				{
					statusText = "Project: " + m_project.getRoot().filename().string();
				}
				else
				{
					statusText = "No Project Loaded";
				}

				if (!m_currentScenePath.empty())
				{
					statusText += " | Scene: " + std::filesystem::path(m_currentScenePath).filename().string();
				}
				else
				{
					statusText += " | Untitled Scene";
				}
				if (m_isSceneDirty)
				{
					statusText += "*";
				}

				float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
				ImGui::SameLine(ImGui::GetWindowWidth() - textWidth - 20.0f);
				ImGui::TextDisabled("%s", statusText.c_str());

				ImGui::EndMainMenuBar();
			}
		}

		// =====================================================================
		// UI: Left Panel (Asset Browser & Hierarchy)
		// =====================================================================
		void renderLeftPanel()
		{
			const float winH = ImGui::GetIO().DisplaySize.y;
			const float panelH = winH - m_menuBarHeight; // Full height to window bottom!

			ImGui::SetNextWindowPos(ImVec2(0.0f, m_menuBarHeight), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, panelH), ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			if (ImGui::Begin("##LeftPanel", nullptr, flags))
			{
				if (ImGui::BeginTabBar("##LeftTabs"))
				{
					if (ImGui::BeginTabItem("Assets"))
					{
						renderAssetBrowserTab();
						ImGui::EndTabItem();
					}
					if (ImGui::BeginTabItem("Hierarchy"))
					{
						renderHierarchyTab();
						ImGui::EndTabItem();
					}
					ImGuiTabItemFlags matTabFlags = 0;
					if (m_selectMaterialsTab)
					{
						matTabFlags |= ImGuiTabItemFlags_SetSelected;
						m_selectMaterialsTab = false;
					}
					if (ImGui::BeginTabItem("Materials", nullptr, matTabFlags))
					{
						renderMaterialsTab();
						ImGui::EndTabItem();
					}
					ImGui::EndTabBar();
				}
			}
			ImGui::End();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
		}

		void renderAssetBrowserTab()
		{
			if (!m_project.isOpen())
			{
				ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "No project open.");
				if (ImGui::Button("Open Project Folder...", ImVec2(-1.0f, 30.0f)))
				{
					m_project.openFolderDialog();
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
				bool active = (m_assetFilterType == i);
				if (active)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.8f, 1.0f));
				if (ImGui::Button(filters[i]))
				{
					m_assetFilterType = i;
				}
				if (active)
					ImGui::PopStyleColor();
			}

			// Search
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_assetSearchBuf, sizeof(m_assetSearchBuf));

			ImGui::Separator();

			// Rescan button
			if (ImGui::Button("Rescan Folder", ImVec2(-1.0f, 22.0f)))
			{
				m_project.scanAssets();
			}

			ImGui::BeginChild("##AssetList", ImVec2(0.0f, 0.0f), true);

			std::string searchStr(m_assetSearchBuf);
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
			if (m_assetFilterType == 0 || m_assetFilterType == 1)
			{
				if (ImGui::CollapsingHeader("Scenes (.weirdscene)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (const auto& sceneAsset : m_project.getScenes())
					{
						if (!matchesSearch(sceneAsset))
							continue;
						ImGui::Bullet();
						if (ImGui::Selectable(sceneAsset.fileName.c_str()))
						{
							loadScene(sceneAsset.fullPath.string());
						}
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("Click to load: %s", sceneAsset.relativePath.c_str());
						}
					}
				}
			}

			// 2. Clusters
			if (m_assetFilterType == 0 || m_assetFilterType == 2)
			{
				if (ImGui::CollapsingHeader("Clusters (.weirdcluster)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Button("+ New Cluster in Assets", ImVec2(-1.0f, 22.0f)))
					{
						m_openNewClusterModal = true;
					}

					for (const auto& clusterAsset : m_project.getClusters())
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
							spawnCluster(clusterAsset.fullPath.string(), getCameraCenterWorld());
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Edit"))
						{
							launchClusterEditor(clusterAsset.fullPath.string());
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Reload"))
						{
							reloadClustersFromAsset(clusterAsset.fullPath.string());
						}
						if (ImGui::BeginPopupContextItem())
						{
							if (ImGui::MenuItem("Spawn in Scene"))
							{
								spawnCluster(clusterAsset.fullPath.string(), getCameraCenterWorld());
							}
							if (ImGui::MenuItem("Edit in Cluster Editor"))
							{
								launchClusterEditor(clusterAsset.fullPath.string());
							}
							if (ImGui::MenuItem("Reload Instances in Scene"))
							{
								reloadClustersFromAsset(clusterAsset.fullPath.string());
							}
							ImGui::EndPopup();
						}
						ImGui::PopID();
					}
				}
			}

			// 3. Custom Shapes
			if (m_assetFilterType == 0 || m_assetFilterType == 3)
			{
				if (ImGui::CollapsingHeader("Custom Shapes (.weirdshape)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (ImGui::Button("+ New Shape in Assets", ImVec2(-1.0f, 22.0f)))
					{
						m_openNewShapeModal = true;
					}

					for (const auto& shapeAsset : m_project.getCustomShapes())
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
							spawnCustomShape(shapeAsset.fullPath.string(), getCameraCenterWorld());
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Edit"))
						{
							launchShapeEditor(shapeAsset.fullPath.string());
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("Reload"))
						{
							reloadCustomShape(shapeAsset.fullPath.string());
						}
						if (ImGui::BeginPopupContextItem())
						{
							if (ImGui::MenuItem("Add to Scene"))
							{
								spawnCustomShape(shapeAsset.fullPath.string(), getCameraCenterWorld());
							}
							if (ImGui::MenuItem("Edit in Shape Editor"))
							{
								launchShapeEditor(shapeAsset.fullPath.string());
							}
							if (ImGui::MenuItem("Reload in Scene"))
							{
								reloadCustomShape(shapeAsset.fullPath.string());
							}
							ImGui::EndPopup();
						}
						ImGui::PopID();
					}
				}
			}

			// Modals for creating new assets
			if (m_openNewClusterModal)
			{
				ImGui::OpenPopup("Create New Cluster");
				m_openNewClusterModal = false;
			}
			if (ImGui::BeginPopupModal("Create New Cluster", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Enter name for the new cluster asset:");
				bool enterPressed = ImGui::InputText("##NewClusterName", m_newClusterNameBuf,
													 sizeof(m_newClusterNameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::Spacing();
				if (ImGui::Button("Create & Open", ImVec2(120.0f, 0.0f)) || enterPressed)
				{
					createNewClusterAsset(m_newClusterNameBuf);
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(80.0f, 0.0f)))
				{
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}

			if (m_openNewShapeModal)
			{
				ImGui::OpenPopup("Create New Shape");
				m_openNewShapeModal = false;
			}
			if (ImGui::BeginPopupModal("Create New Shape", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("Enter name for the new custom shape asset:");
				bool enterPressed = ImGui::InputText("##NewShapeName", m_newShapeNameBuf, sizeof(m_newShapeNameBuf),
													 ImGuiInputTextFlags_EnterReturnsTrue);
				ImGui::Spacing();
				if (ImGui::Button("Create & Open", ImVec2(120.0f, 0.0f)) || enterPressed)
				{
					createNewShapeAsset(m_newShapeNameBuf);
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

		void renderHierarchyTab()
		{
			// 1. Search Bar
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##HierSearch", "Filter hierarchy...", m_hierarchySearchBuf,
									 sizeof(m_hierarchySearchBuf));

			// 2. Toolbar
			if (ImGui::Button("+ New Group"))
			{
				createGroup();
			}
			ImGui::SameLine();
			bool canGroup = canGroupSelected();
			if (!canGroup)
				ImGui::BeginDisabled();
			if (ImGui::Button("Group (Ctrl+G)"))
			{
				groupSelected();
			}
			if (!canGroup)
				ImGui::EndDisabled();

			ImGui::SameLine();
			bool canUngroup = (m_selectedGroupId != 0);
			if (!canUngroup)
				ImGui::BeginDisabled();
			if (ImGui::Button("Ungroup"))
			{
				ungroupSelected();
			}
			if (!canUngroup)
				ImGui::EndDisabled();

			size_t selCount = m_selectedEntities.size() + m_selectedClusterIds.size();
			if (selCount > 1)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "(%zu selected)", selCount);
				ImGui::SameLine();
				if (ImGui::SmallButton("Clear"))
				{
					deselectAll();
				}
			}

			ImGui::Separator();

			ImGui::BeginChild("##HierarchyList", ImVec2(0.0f, 0.0f), false);

			cleanGroups();
			m_visibleHierarchyItems.clear();

			std::string searchStr(m_hierarchySearchBuf);
			std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

			std::unordered_set<uint32_t> groupedClusterIds;
			std::unordered_set<Entity> groupedEntities;
			for (const auto& g : m_groups)
			{
				for (uint32_t cId : g.clusterIds)
					groupedClusterIds.insert(cId);
				for (Entity e : g.entities)
					groupedEntities.insert(e);
			}

			// A) Groups
			for (auto& group : m_groups)
			{
				renderHierarchyGroupItem(group, searchStr);
			}

			// B) Root Clusters (clusters not in any group)
			const auto& allClusters = m_services->clusters().getAll();
			for (size_t cIdx = 0; cIdx < allClusters.size(); ++cIdx)
			{
				const auto& cluster = allClusters[cIdx];
				if (groupedClusterIds.count(cluster.id) == 0)
				{
					renderHierarchyClusterItem(cluster, cIdx, searchStr);
				}
			}

			// C) Root Entities (alive entities not in any cluster and not in any group)
			for (Entity e = 0; e < m_registry->getEntityCount(); ++e)
			{
				if (!m_registry->isEntityAlive(e))
					continue;

				if (m_services->clusters().getClusterForEntity(e) != nullptr)
					continue;

				if (groupedEntities.count(e) > 0)
					continue;

				renderHierarchyEntityItem(e, nullptr, searchStr);
			}

			// Context menu on empty area
			if (ImGui::BeginPopupContextWindow("HierarchyEmptyContextMenu",
											   ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				if (ImGui::MenuItem("+ New Group"))
				{
					createGroup();
				}
				if (ImGui::MenuItem("Deselect All", "Esc", false, hasSelection()))
				{
					deselectAll();
				}
				ImGui::EndPopup();
			}

			if (m_hasPendingShift)
			{
				applyRangeSelection(m_shiftAnchorItem, m_pendingShiftTarget, m_pendingShiftIsCtrl);
				m_hasPendingShift = false;
			}

			ImGui::EndChild();
		}

		void renderHierarchyEntityItem(Entity e, const ClusterInstance* parentCluster,
									   const std::string& searchStr = "")
		{
			std::string label = formatEntityLabel(e, parentCluster);

			if (!searchStr.empty())
			{
				std::string lLower = label;
				std::transform(lLower.begin(), lLower.end(), lLower.begin(), ::tolower);
				if (lLower.find(searchStr) == std::string::npos)
					return;
			}

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isEntitySelected(e))
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::PushID(static_cast<int>(e));
			ImGui::TreeNodeEx((void*)(uintptr_t)e, flags, "%s", label.c_str());

			m_visibleHierarchyItems.push_back({HierarchyItemType::Entity, static_cast<uint32_t>(e)});

			if (ImGui::IsItemHovered())
			{
				auto compIDs = m_registry->getComponentTypes(e);
				if (!compIDs.empty())
				{
					ImGui::BeginTooltip();
					ImGui::TextDisabled("Attached Components:");
					for (size_t compID : compIDs)
					{
						std::string compName = m_registry->getComponentName(compID);
						if (compName == "Unknown")
						{
							ImGui::BulletText("Missing Script / Unknown (Type ID: %zu)", compID);
						}
						else
						{
							ImGui::BulletText("%s", compName.c_str());
						}
					}
					ImGui::EndTooltip();
				}
			}

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				handleHierarchyItemClick({HierarchyItemType::Entity, static_cast<uint32_t>(e)});
			}

			// Drag source
			if (parentCluster == nullptr && ImGui::BeginDragDropSource())
			{
				Entity payload = e;
				ImGui::SetDragDropPayload("HIER_ENTITY", &payload, sizeof(Entity));
				ImGui::Text("%s", label.c_str());
				ImGui::EndDragDropSource();
			}

			// Context menu
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Select"))
				{
					selectEntity(e, false);
				}
				if (canGroupSelected())
				{
					if (ImGui::MenuItem("Group Selected (Ctrl+G)"))
					{
						groupSelected();
					}
				}
				if (m_selectedEntities.size() == 2)
				{
					ImGui::Separator();
					if (ImGui::MenuItem("Create Spring Between Selected (2)"))
					{
						auto it = m_selectedEntities.begin();
						Entity entA = *it++;
						Entity entB = *it;
						Entity springEnt = m_registry->createEntity();
						m_services->names().setName(springEnt,
													"Spring_" + std::to_string(entA) + "_" + std::to_string(entB));
						auto& sp = m_registry->addComponent<Spring>(springEnt);
						sp.entityA = entA;
						sp.entityB = entB;
						vec2 posA = getEntityPosition(entA);
						vec2 posB = getEntityPosition(entB);
						sp.restDistance = glm::distance(posA, posB);
						sp.stiffness = 1.0f;
						m_registry->setComponentDirty(sp);
						syncConstraintClusterMembership(springEnt, entA, entB);
						selectEntity(springEnt, false);
						m_isSceneDirty = true;
					}
					if (ImGui::MenuItem("Create Distance Constraint Between Selected (2)"))
					{
						auto it = m_selectedEntities.begin();
						Entity entA = *it++;
						Entity entB = *it;
						Entity dcEnt = m_registry->createEntity();
						m_services->names().setName(dcEnt, "DistanceConstraint_" + std::to_string(entA) + "_" +
															   std::to_string(entB));
						auto& dc = m_registry->addComponent<DistanceConstraint>(dcEnt);
						dc.entityA = entA;
						dc.entityB = entB;
						vec2 posA = getEntityPosition(entA);
						vec2 posB = getEntityPosition(entB);
						dc.distance = glm::distance(posA, posB);
						m_registry->setComponentDirty(dc);
						syncConstraintClusterMembership(dcEnt, entA, entB);
						selectEntity(dcEnt, false);
						m_isSceneDirty = true;
					}
				}
				if (parentCluster == nullptr && !m_groups.empty())
				{
					if (ImGui::BeginMenu("Add to Group"))
					{
						for (const auto& g : m_groups)
						{
							if (ImGui::MenuItem(g.name.c_str()))
							{
								addEntityToGroup(g.id, e);
							}
						}
						ImGui::EndMenu();
					}
				}
				if (isEntityInAnyGroup(e))
				{
					if (ImGui::MenuItem("Remove from Group"))
					{
						removeFromAllGroups(e);
					}
				}
				if (e != m_services->render().getCameraEntity())
				{
					ImGui::Separator();
					if (ImGui::MenuItem("Delete Entity", "Del"))
					{
						selectEntity(e, false);
						deleteSelected();
					}
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		void renderHierarchyClusterItem(const ClusterInstance& cluster, size_t clusterIdx = 0,
										const std::string& searchStr = "")
		{
			if (!searchStr.empty())
			{
				std::string cLower = cluster.name + " " + cluster.type;
				std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
				bool anyMatch = (cLower.find(searchStr) != std::string::npos);
				if (!anyMatch)
				{
					for (Entity member : cluster.entities)
					{
						std::string mLabel = formatEntityLabel(member, &cluster);
						std::transform(mLabel.begin(), mLabel.end(), mLabel.begin(), ::tolower);
						if (mLabel.find(searchStr) != std::string::npos)
						{
							anyMatch = true;
							break;
						}
					}
				}
				if (!anyMatch)
					return;
			}

			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (isClusterSelected(cluster.id))
			{
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			std::string clusterLabel = "[Cluster] " + cluster.name + " (" + cluster.type + ")";

			ImGui::PushID(static_cast<int>(100000 + cluster.id * 1000 + clusterIdx));
			bool open = ImGui::TreeNodeEx((void*)(uintptr_t)(100000 + cluster.id * 1000 + clusterIdx), flags, "%s",
										  clusterLabel.c_str());

			m_visibleHierarchyItems.push_back({HierarchyItemType::Cluster, cluster.id});

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				handleHierarchyItemClick({HierarchyItemType::Cluster, cluster.id});
			}

			// Drag source
			if (ImGui::BeginDragDropSource())
			{
				uint32_t payload = cluster.id;
				ImGui::SetDragDropPayload("HIER_CLUSTER", &payload, sizeof(uint32_t));
				ImGui::Text("[Cluster] %s", cluster.name.c_str());
				ImGui::EndDragDropSource();
			}

			// Context menu
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Select Cluster"))
				{
					selectCluster(cluster.id, false);
				}
				if (!cluster.assetPath.empty())
				{
					if (ImGui::MenuItem("Reload from File"))
					{
						reloadCluster(cluster.id);
					}
					if (ImGui::MenuItem("Edit in Cluster Editor"))
					{
						launchClusterEditor(cluster.assetPath);
					}
				}
				if (ImGui::MenuItem("Reset to Initial Position (R)"))
				{
					auto* c = m_services->clusters().getById(cluster.id);
					if (c)
						resetClusterToInitialPosition(*c);
				}
				if (canGroupSelected())
				{
					if (ImGui::MenuItem("Group Selected (Ctrl+G)"))
					{
						groupSelected();
					}
				}
				if (!m_groups.empty())
				{
					if (ImGui::BeginMenu("Add to Group"))
					{
						for (const auto& g : m_groups)
						{
							if (ImGui::MenuItem(g.name.c_str()))
							{
								addClusterToGroup(g.id, cluster.id);
							}
						}
						ImGui::EndMenu();
					}
				}
				if (isClusterInAnyGroup(cluster.id))
				{
					if (ImGui::MenuItem("Remove from Group"))
					{
						removeClusterFromAllGroups(cluster.id);
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Destroy Cluster", "Del"))
				{
					selectCluster(cluster.id, false);
					deleteSelected();
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				for (Entity e : cluster.entities)
				{
					if (m_registry->isEntityAlive(e))
					{
						renderHierarchyEntityItem(e, &cluster, searchStr);
					}
				}
				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		void renderHierarchyGroupItem(EditorGroup& group, const std::string& searchStr = "")
		{
			if (!searchStr.empty())
			{
				std::string gLower = group.name;
				std::transform(gLower.begin(), gLower.end(), gLower.begin(), ::tolower);
				bool match = (gLower.find(searchStr) != std::string::npos);
				if (!match)
				{
					for (uint32_t cId : group.clusterIds)
					{
						auto* c = m_services->clusters().getById(cId);
						if (c)
						{
							std::string cLower = c->name;
							std::transform(cLower.begin(), cLower.end(), cLower.begin(), ::tolower);
							if (cLower.find(searchStr) != std::string::npos)
							{
								match = true;
								break;
							}
						}
					}
					if (!match)
					{
						for (Entity e : group.entities)
						{
							std::string eLower = formatEntityLabel(e);
							std::transform(eLower.begin(), eLower.end(), eLower.begin(), ::tolower);
							if (eLower.find(searchStr) != std::string::npos)
							{
								match = true;
								break;
							}
						}
					}
				}
				if (!match)
					return;
			}

			std::string groupLabel = "[Group] " + group.name + "##g_" + std::to_string(group.id);
			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;

			ImGui::PushID(static_cast<int>(500000 + group.id));
			bool open = ImGui::TreeNodeEx((void*)(uintptr_t)(500000 + group.id), flags, "%s", groupLabel.c_str());

			m_visibleHierarchyItems.push_back({HierarchyItemType::Group, group.id});

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
			{
				handleHierarchyItemClick({HierarchyItemType::Group, group.id});
			}

			// Drag-and-drop target to drop items into this group
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_ENTITY"))
				{
					Entity e = *static_cast<const Entity*>(payload->Data);
					addEntityToGroup(group.id, e);
				}
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIER_CLUSTER"))
				{
					uint32_t cId = *static_cast<const uint32_t*>(payload->Data);
					addClusterToGroup(group.id, cId);
				}
				ImGui::EndDragDropTarget();
			}

			// Context menu
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Select Group"))
				{
					selectGroup(group.id);
				}
				if (hasSelection())
				{
					if (ImGui::MenuItem("Add Selected Items to This Group"))
					{
						for (Entity e : m_selectedEntities)
							addEntityToGroup(group.id, e);
						for (uint32_t cId : m_selectedClusterIds)
							addClusterToGroup(group.id, cId);
					}
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G"))
				{
					selectGroup(group.id);
					ungroupSelected();
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				for (size_t cIdx = 0; cIdx < group.clusterIds.size(); ++cIdx)
				{
					uint32_t cId = group.clusterIds[cIdx];
					auto* c = m_services->clusters().getById(cId);
					if (c)
					{
						renderHierarchyClusterItem(*c, cIdx, searchStr);
					}
				}

				for (Entity e : group.entities)
				{
					if (m_registry->isEntityAlive(e))
					{
						renderHierarchyEntityItem(e, nullptr, searchStr);
					}
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		// =====================================================================
		// UI: Materials Management & Cluster Reassignment
		// =====================================================================
		std::string getMaterialName(uint16_t id) const
		{
			if (id >= 16)
				id = 15;
			const auto& mat = m_services->materials2D().get(id);
			if (!mat.name.empty())
				return "[" + std::to_string(id) + "] " + mat.name;
			return "Material " + std::to_string(id);
		}

		bool renderMaterialSelector(const char* label, uint16_t& currentMat)
		{
			bool changed = false;
			uint16_t safeId = currentMat < 16 ? currentMat : 15;
			const auto& curMat = m_services->materials2D().get(safeId);

			ImGui::PushID(label);
			ImGui::ColorButton("##cur_swatch", ImVec4(curMat.color.r, curMat.color.g, curMat.color.b, curMat.color.a),
							   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18.0f, 18.0f));
			ImGui::SameLine();

			std::string comboPreview = getMaterialName(safeId);
			if (ImGui::BeginCombo(label, comboPreview.c_str()))
			{
				for (int i = 0; i < 16; ++i)
				{
					ImGui::PushID(i);
					const auto& m = m_services->materials2D().get(static_cast<uint16_t>(i));
					ImGui::ColorButton("##combo_swatch", ImVec4(m.color.r, m.color.g, m.color.b, m.color.a),
									   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
									   ImVec2(14.0f, 14.0f));
					ImGui::SameLine();
					bool isSel = (safeId == static_cast<uint16_t>(i));
					if (ImGui::Selectable(getMaterialName(static_cast<uint16_t>(i)).c_str(), isSel))
					{
						currentMat = static_cast<uint16_t>(i);
						changed = true;
					}
					if (isSel)
					{
						ImGui::SetItemDefaultFocus();
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::PopID();
			return changed;
		}

		bool renderMaterialSelector(const char* label, unsigned int& currentMat)
		{
			uint16_t m = static_cast<uint16_t>(currentMat);
			if (renderMaterialSelector(label, m))
			{
				currentMat = static_cast<unsigned int>(m);
				return true;
			}
			return false;
		}

		void reassignClusterMaterial(ClusterInstance& cluster, uint16_t sourceMat, uint16_t targetMat)
		{
			bool changed = false;
			for (Entity e : cluster.entities)
			{
				if (m_registry->hasComponent<Shape>(e))
				{
					auto& s = m_registry->getComponent<Shape>(e);
					if (s.material == sourceMat)
					{
						s.material = targetMat;
						m_registry->setComponentDirty(s);
						changed = true;
					}
				}
				if (m_registry->hasComponent<Dot>(e))
				{
					auto& dot = m_registry->getComponent<Dot>(e);
					if (dot.materialId == sourceMat)
					{
						dot.materialId = static_cast<unsigned int>(targetMat);
						m_registry->setComponentDirty(dot);
						changed = true;
					}
				}
				if (m_registry->hasComponent<TextRenderer>(e))
				{
					auto& tr = m_registry->getComponent<TextRenderer>(e);
					if (tr.material == sourceMat)
					{
						tr.material = targetMat;
						m_registry->setComponentDirty(tr);
						changed = true;
					}
				}
			}
			if (changed)
			{
				m_isSceneDirty = true;
				m_2DWorldRenderContext.shapesNeedUpdate = true;
			}
		}

		void setClusterAllMaterials(ClusterInstance& cluster, uint16_t targetMat)
		{
			bool changed = false;
			for (Entity e : cluster.entities)
			{
				if (m_registry->hasComponent<Shape>(e))
				{
					auto& s = m_registry->getComponent<Shape>(e);
					if (s.material != targetMat)
					{
						s.material = targetMat;
						m_registry->setComponentDirty(s);
						changed = true;
					}
				}
				if (m_registry->hasComponent<Dot>(e))
				{
					auto& dot = m_registry->getComponent<Dot>(e);
					if (dot.materialId != targetMat)
					{
						dot.materialId = static_cast<unsigned int>(targetMat);
						m_registry->setComponentDirty(dot);
						changed = true;
					}
				}
				if (m_registry->hasComponent<TextRenderer>(e))
				{
					auto& tr = m_registry->getComponent<TextRenderer>(e);
					if (tr.material != targetMat)
					{
						tr.material = targetMat;
						m_registry->setComponentDirty(tr);
						changed = true;
					}
				}
			}
			if (changed)
			{
				m_isSceneDirty = true;
				m_2DWorldRenderContext.shapesNeedUpdate = true;
			}
		}

		void renderMaterialsTab()
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Scene Materials (16 Slots)");
			ImGui::TextDisabled("Customize 2D materials used in the scene.");
			ImGui::Separator();

			// Swatches grid (2 rows of 8)
			const float swatchSize = 28.0f;
			const float spacing = 4.0f;
			for (int i = 0; i < 16; ++i)
			{
				if (i > 0 && i % 8 != 0)
				{
					ImGui::SameLine(0.0f, spacing);
				}

				ImGui::PushID(i);
				const bool selected = (m_selectedMaterialSlot == i);
				const auto& mat = m_services->materials2D().get(static_cast<uint16_t>(i));

				if (selected)
				{
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
					ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				}

				if (ImGui::ColorButton(
						"##mat_palette_swatch", ImVec4(mat.color.r, mat.color.g, mat.color.b, mat.color.a),
						ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(swatchSize, swatchSize)))
				{
					m_selectedMaterialSlot = i;
				}

				if (selected)
				{
					ImGui::PopStyleColor();
					ImGui::PopStyleVar();
				}

				if (ImGui::IsItemHovered())
				{
					if (!mat.name.empty())
					{
						ImGui::SetTooltip("[%d] %s%s", i, mat.name.c_str(), selected ? " (selected)" : "");
					}
					else
					{
						ImGui::SetTooltip("Material %d%s", i, selected ? " (selected)" : "");
					}
				}

				ImGui::PopID();
			}

			ImGui::Spacing();
			ImGui::Separator();

			// Selected Material Properties
			if (m_selectedMaterialSlot < 0 || m_selectedMaterialSlot >= 16)
			{
				m_selectedMaterialSlot = 0;
			}

			auto& mat = m_services->materials2D().get(static_cast<uint16_t>(m_selectedMaterialSlot));

			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Slot [%d] Properties", m_selectedMaterialSlot);

			// Name
			char nameBuf[64];
			std::snprintf(nameBuf, sizeof(nameBuf), "%s", mat.name.c_str());
			if (ImGui::InputText("Name##SlotName", nameBuf, sizeof(nameBuf)))
			{
				mat.name = nameBuf;
				m_isSceneDirty = true;
			}

			// Color
			if (ImGui::ColorEdit4("Color##SlotColor", &mat.color[0], ImGuiColorEditFlags_AlphaBar))
			{
				m_isSceneDirty = true;
				m_2DWorldRenderContext.shapesNeedUpdate = true;
			}

			// Copy / Paste color
			if (ImGui::SmallButton("Copy Color"))
			{
				m_copiedMaterialColor = mat.color;
				m_hasCopiedMaterialColor = true;
			}
			if (m_hasCopiedMaterialColor)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Paste Color"))
				{
					mat.color = m_copiedMaterialColor;
					m_isSceneDirty = true;
					m_2DWorldRenderContext.shapesNeedUpdate = true;
				}
			}

			// Pattern
			const char* patternNames[] = {"None", "Checkers", "Perlin Noise", "Waves", "Gradient"};
			int currentPattern = static_cast<int>(mat.pattern);
			if (currentPattern < 0 || currentPattern > 4)
			{
				currentPattern = 0;
			}

			if (ImGui::Combo("Pattern##SlotPattern", &currentPattern, patternNames, 5))
			{
				mat.pattern = static_cast<Pattern2D>(currentPattern);
				m_isSceneDirty = true;
				m_2DWorldRenderContext.shapesNeedUpdate = true;
			}

			if (mat.pattern != Pattern2D::None)
			{
				if (ImGui::ColorEdit4("Secondary Color##SlotSecColor", &mat.secondaryColor[0],
									  ImGuiColorEditFlags_AlphaBar))
				{
					m_isSceneDirty = true;
					m_2DWorldRenderContext.shapesNeedUpdate = true;
				}
				if (ImGui::DragFloat("Pattern Scale##SlotScale", &mat.patternScale, 0.05f, 0.01f, 100.0f))
				{
					m_isSceneDirty = true;
					m_2DWorldRenderContext.shapesNeedUpdate = true;
				}
			}

			ImGui::Spacing();
			if (ImGui::Button("Reset Slot to Default", ImVec2(-1.0f, 22.0f)))
			{
				if (static_cast<size_t>(m_selectedMaterialSlot) < ColorPalette::Default.size())
				{
					mat.color = vec4(ColorPalette::Default[m_selectedMaterialSlot], 1.0f);
				}
				else
				{
					mat.color = vec4(1.0f);
				}
				mat.secondaryColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
				mat.pattern = Pattern2D::None;
				mat.patternScale = 1.0f;
				m_isSceneDirty = true;
				m_2DWorldRenderContext.shapesNeedUpdate = true;
			}

			ImGui::Spacing();
			ImGui::Separator();

			// All slots summary list
			ImGui::Text("All Slots:");
			ImGui::BeginChild("##MaterialSlotsList", ImVec2(0.0f, 0.0f), true);
			for (int i = 0; i < 16; ++i)
			{
				ImGui::PushID(i);
				const auto& slotMat = m_services->materials2D().get(static_cast<uint16_t>(i));
				bool isSelected = (m_selectedMaterialSlot == i);

				ImGui::ColorButton(
					"##row_swatch", ImVec4(slotMat.color.r, slotMat.color.g, slotMat.color.b, slotMat.color.a),
					ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(16.0f, 16.0f));
				ImGui::SameLine();

				std::string rowLabel = "[" + std::to_string(i) + "] ";
				if (!slotMat.name.empty())
				{
					rowLabel += slotMat.name;
				}
				else
				{
					rowLabel += "(unnamed)";
				}
				if (slotMat.pattern != Pattern2D::None)
				{
					rowLabel += " [Pattern]";
				}

				if (ImGui::Selectable(rowLabel.c_str(), isSelected))
				{
					m_selectedMaterialSlot = i;
				}

				ImGui::PopID();
			}
			ImGui::EndChild();
		}

		// =====================================================================
		// UI: Right Panel (Inspector)
		// =====================================================================
		void renderInspector()
		{
			const float winW = ImGui::GetIO().DisplaySize.x;
			const float winH = ImGui::GetIO().DisplaySize.y;
			const float panelH = winH - m_menuBarHeight; // Full height to window bottom!

			ImGui::SetNextWindowPos(ImVec2(winW - RIGHT_PANEL_WIDTH, m_menuBarHeight), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_WIDTH, panelH), ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			if (ImGui::Begin("##InspectorPanel", nullptr, flags))
			{
				ImGui::TextDisabled("INSPECTOR");
				ImGui::Separator();

				if (m_selectedGroupId != 0)
				{
					renderGroupInspector();
				}
				else if (m_selectedEntities.size() + m_selectedClusterIds.size() > 1)
				{
					renderMultiSelectionInspector();
				}
				else if (m_selectedClusterId != 0)
				{
					renderClusterInspector();
				}
				else if (m_selectedEntity != INVALID_ENTITY)
				{
					renderEntityInspector();
				}
				else
				{
					ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No entity or cluster selected.");
					ImGui::Spacing();
					ImGui::TextWrapped("Click in the viewport or hierarchy to select an object.");
				}
			}
			ImGui::End();
			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
		}

		void renderGroupInspector()
		{
			auto* g = findGroup(m_selectedGroupId);
			if (!g)
			{
				m_selectedGroupId = 0;
				return;
			}

			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Group (Dummy Container)");
			ImGui::TextDisabled("ID: %u | %zu Entities, %zu Clusters", g->id, g->entities.size(), g->clusterIds.size());
			ImGui::Separator();

			char nameBuf[128];
			std::snprintf(nameBuf, sizeof(nameBuf), "%s", g->name.c_str());
			if (ImGui::InputText("Group Name", nameBuf, sizeof(nameBuf)))
			{
				g->name = nameBuf;
				m_isSceneDirty = true;
			}

			ImGui::Spacing();
			if (ImGui::Button("Ungroup (Ctrl+Shift+G)", ImVec2(-1.0f, 26.0f)))
			{
				ungroupSelected();
				return;
			}

			if (canGroupSelected())
			{
				if (ImGui::Button("Add Selected Items to This Group", ImVec2(-1.0f, 24.0f)))
				{
					for (Entity e : m_selectedEntities)
						addEntityToGroup(g->id, e);
					for (uint32_t cId : m_selectedClusterIds)
						addClusterToGroup(g->id, cId);
				}
			}

			ImGui::Separator();
			ImGui::Text("Contents (%zu items):", g->clusterIds.size() + g->entities.size());

			for (size_t i = 0; i < g->clusterIds.size();)
			{
				uint32_t cId = g->clusterIds[i];
				auto* c = m_services->clusters().getById(cId);
				std::string label = c ? ("[Cluster] " + c->name) : ("Cluster #" + std::to_string(cId));
				ImGui::BulletText("%s", label.c_str());
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
				ImGui::PushID(static_cast<int>(1000 + cId));
				if (ImGui::SmallButton("Remove"))
				{
					g->clusterIds.erase(g->clusterIds.begin() + i);
					m_isSceneDirty = true;
				}
				else
				{
					++i;
				}
				ImGui::PopID();
			}

			for (size_t i = 0; i < g->entities.size();)
			{
				Entity e = g->entities[i];
				std::string label = formatEntityLabel(e);
				ImGui::BulletText("%s", label.c_str());
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
				ImGui::PushID(static_cast<int>(2000 + e));
				if (ImGui::SmallButton("Remove"))
				{
					g->entities.erase(g->entities.begin() + i);
					m_isSceneDirty = true;
				}
				else
				{
					++i;
				}
				ImGui::PopID();
			}

			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("Delete Group", ImVec2(-1.0f, 24.0f)))
			{
				ungroupSelected();
			}
		}

		void renderMultiSelectionInspector()
		{
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Multi-Selection");
			ImGui::TextDisabled("%zu Entities, %zu Clusters selected", m_selectedEntities.size(),
								m_selectedClusterIds.size());
			ImGui::Separator();

			if (ImGui::Button("Group Selected (Ctrl+G)", ImVec2(-1.0f, 26.0f)))
			{
				groupSelected();
				return;
			}

			if (m_selectedEntities.size() == 2 && m_selectedClusterIds.empty())
			{
				if (ImGui::Button("Create Spring Between Pair", ImVec2(-1.0f, 24.0f)))
				{
					auto it = m_selectedEntities.begin();
					Entity entA = *it++;
					Entity entB = *it;
					Entity springEnt = m_registry->createEntity();
					m_services->names().setName(springEnt,
												"Spring_" + std::to_string(entA) + "_" + std::to_string(entB));
					auto& sp = m_registry->addComponent<Spring>(springEnt);
					sp.entityA = entA;
					sp.entityB = entB;
					vec2 posA = getEntityPosition(entA);
					vec2 posB = getEntityPosition(entB);
					sp.restDistance = glm::distance(posA, posB);
					sp.stiffness = 1.0f;
					m_registry->setComponentDirty(sp);
					syncConstraintClusterMembership(springEnt, entA, entB);
					selectEntity(springEnt, false);
					m_isSceneDirty = true;
					return;
				}
				if (ImGui::Button("Create Distance Constraint Between Pair", ImVec2(-1.0f, 24.0f)))
				{
					auto it = m_selectedEntities.begin();
					Entity entA = *it++;
					Entity entB = *it;
					Entity dcEnt = m_registry->createEntity();
					m_services->names().setName(dcEnt, "DistanceConstraint_" + std::to_string(entA) + "_" +
														   std::to_string(entB));
					auto& dc = m_registry->addComponent<DistanceConstraint>(dcEnt);
					dc.entityA = entA;
					dc.entityB = entB;
					vec2 posA = getEntityPosition(entA);
					vec2 posB = getEntityPosition(entB);
					dc.distance = glm::distance(posA, posB);
					m_registry->setComponentDirty(dc);
					syncConstraintClusterMembership(dcEnt, entA, entB);
					selectEntity(dcEnt, false);
					m_isSceneDirty = true;
					return;
				}
			}

			if (ImGui::Button("Delete Selected (Del)", ImVec2(-1.0f, 24.0f)))
			{
				deleteSelected();
				return;
			}

			if (ImGui::Button("Deselect All (Esc)", ImVec2(-1.0f, 22.0f)))
			{
				deselectAll();
				return;
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Selected Items:");

			for (uint32_t cId : m_selectedClusterIds)
			{
				auto* c = m_services->clusters().getById(cId);
				std::string label = c ? ("[Cluster] " + c->name) : ("Cluster #" + std::to_string(cId));
				ImGui::BulletText("%s", label.c_str());
			}

			for (Entity e : m_selectedEntities)
			{
				std::string label = formatEntityLabel(e);
				ImGui::BulletText("%s", label.c_str());
			}
		}

		void renderClusterInspector()
		{
			auto* cluster = m_services->clusters().getById(m_selectedClusterId);
			if (!cluster)
			{
				m_selectedClusterId = 0;
				return;
			}

			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Cluster Instance");
			ImGui::TextDisabled("ID: %u | Type: %s", cluster->id, cluster->type.c_str());
			if (!cluster->assetPath.empty())
			{
				ImGui::TextDisabled("Asset: %s", std::filesystem::path(cluster->assetPath).filename().string().c_str());
				float avail = ImGui::GetContentRegionAvail().x;
				if (ImGui::Button("Edit in Cluster Editor", ImVec2(avail * 0.58f - 4.0f, 22.0f)))
				{
					launchClusterEditor(cluster->assetPath);
				}
				ImGui::SameLine();
				if (ImGui::Button("Reload from File", ImVec2(-1.0f, 22.0f)))
				{
					reloadCluster(cluster->id);
				}
			}
			ImGui::Separator();

			// Cluster Name
			std::snprintf(m_clusterNameBuf, sizeof(m_clusterNameBuf), "%s", cluster->name.c_str());
			if (ImGui::InputText("Name", m_clusterNameBuf, sizeof(m_clusterNameBuf)))
			{
				cluster->name = m_clusterNameBuf;
				m_isSceneDirty = true;
			}

			// Reset Cluster Buttons
			ImGui::Spacing();
			if (ImGui::Button("Reset to Initial Position (R)", ImVec2(-1.0f, 26.0f)))
			{
				resetClusterToInitialPosition(*cluster);
			}
			if (ImGui::Button("Reset Shape at Current Centroid", ImVec2(-1.0f, 24.0f)))
			{
				vec2 centroid = computeClusterCentroid(*cluster);
				resetClusterShapeAtPosition(*cluster, centroid);
			}

			// Cluster Tags
			ImGui::Spacing();
			ImGui::Text("Tags:");
			for (size_t i = 0; i < cluster->tags.size();)
			{
				ImGui::PushID(static_cast<int>(i));
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[%s]", cluster->tags[i].c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("x"))
				{
					cluster->tags.erase(cluster->tags.begin() + i);
					m_isSceneDirty = true;
				}
				else
				{
					ImGui::SameLine();
					++i;
				}
				ImGui::PopID();
			}

			ImGui::NewLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputTextWithHint("##ClusterAddTag", "New tag...", m_newTagBuf, sizeof(m_newTagBuf));
			ImGui::SameLine();
			if (ImGui::Button("Add Tag") && m_newTagBuf[0] != '\0')
			{
				cluster->tags.push_back(m_newTagBuf);
				m_newTagBuf[0] = '\0';
				m_isSceneDirty = true;
			}

			ImGui::Separator();

			// Transform (Centroid position)
			float pos[2] = {cluster->position.x, cluster->position.y};
			if (ImGui::DragFloat2("Position", pos, 0.1f))
			{
				setSelectedPosition(vec2(pos[0], pos[1]));
				m_isSceneDirty = true;
			}

			// Cluster Materials
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
			{
				// Collect unique materials used across member entities
				std::map<uint16_t, int> usedMaterials;
				for (Entity e : cluster->entities)
				{
					if (m_registry->hasComponent<Shape>(e))
					{
						usedMaterials[m_registry->getComponent<Shape>(e).material]++;
					}
					if (m_registry->hasComponent<Dot>(e))
					{
						usedMaterials[static_cast<uint16_t>(m_registry->getComponent<Dot>(e).materialId)]++;
					}
					if (m_registry->hasComponent<TextRenderer>(e))
					{
						usedMaterials[m_registry->getComponent<TextRenderer>(e).material]++;
					}
				}

				// 1. Quick Reassign All palette
				ImGui::Text("Quick Assign All Shapes:");
				const float swatchSize = 22.0f;
				const float spacing = 3.0f;
				for (int i = 0; i < 16; ++i)
				{
					if (i > 0 && i % 8 != 0)
					{
						ImGui::SameLine(0.0f, spacing);
					}
					ImGui::PushID(i);
					const auto& mat = m_services->materials2D().get(static_cast<uint16_t>(i));
					if (ImGui::ColorButton("##ClusterAssignAllSwatch",
										   ImVec4(mat.color.r, mat.color.g, mat.color.b, mat.color.a),
										   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
										   ImVec2(swatchSize, swatchSize)))
					{
						setClusterAllMaterials(*cluster, static_cast<uint16_t>(i));
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Assign ALL entities in cluster to [%d] %s", i,
										  mat.name.empty() ? ("Material " + std::to_string(i)).c_str()
														   : mat.name.c_str());
					}
					ImGui::PopID();
				}

				ImGui::Spacing();

				// 2. Used Materials in cluster & individual remapping
				if (usedMaterials.empty())
				{
					ImGui::TextDisabled("No shapes with materials in cluster.");
				}
				else
				{
					ImGui::Text("Used Materials (%zu):", usedMaterials.size());
					for (const auto& [matId, count] : usedMaterials)
					{
						ImGui::PushID(static_cast<int>(matId));
						const auto& srcMat = m_services->materials2D().get(matId);
						ImGui::ColorButton(
							"##src_swatch", ImVec4(srcMat.color.r, srcMat.color.g, srcMat.color.b, srcMat.color.a),
							ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop, ImVec2(18.0f, 18.0f));
						ImGui::SameLine();
						std::string srcLabel = getMaterialName(matId) + " (" + std::to_string(count) + ")";
						ImGui::TextUnformatted(srcLabel.c_str());

						ImGui::SameLine();
						ImGui::SetNextItemWidth(120.0f);
						std::string comboPreview = getMaterialName(matId);
						if (ImGui::BeginCombo("##remap_combo", comboPreview.c_str()))
						{
							for (int t = 0; t < 16; ++t)
							{
								ImGui::PushID(t);
								const auto& tMat = m_services->materials2D().get(static_cast<uint16_t>(t));
								ImGui::ColorButton("##combo_swatch",
												   ImVec4(tMat.color.r, tMat.color.g, tMat.color.b, tMat.color.a),
												   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoDragDrop,
												   ImVec2(14.0f, 14.0f));
								ImGui::SameLine();
								std::string tName = getMaterialName(static_cast<uint16_t>(t));
								bool isSelected = (matId == static_cast<uint16_t>(t));
								if (ImGui::Selectable(tName.c_str(), isSelected))
								{
									if (static_cast<uint16_t>(t) != matId)
									{
										reassignClusterMaterial(*cluster, matId, static_cast<uint16_t>(t));
									}
								}
								if (isSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
								ImGui::PopID();
							}
							ImGui::EndCombo();
						}
						ImGui::PopID();
					}
				}

				ImGui::Spacing();
				if (ImGui::Button("Customize Scene Materials...", ImVec2(-1.0f, 22.0f)))
				{
					m_selectMaterialsTab = true;
				}
			}

			// Member Entities & Local Names (clickable to inspect individual entities)
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Member Entities", ImGuiTreeNodeFlags_DefaultOpen))
			{
				std::unordered_map<Entity, std::string> entityToLocalName;
				for (const auto& [localName, entity] : cluster->localNames)
				{
					entityToLocalName[entity] = localName;
				}

				for (Entity e : cluster->entities)
				{
					std::string entLabel;
					auto itName = entityToLocalName.find(e);
					if (itName != entityToLocalName.end())
					{
						entLabel = itName->second + " (#" + std::to_string(static_cast<uint32_t>(e)) + ")";
					}
					else
					{
						entLabel = "Entity #" + std::to_string(static_cast<uint32_t>(e));
					}

					if (m_registry->hasComponent<RigidBody2D>(e))
					{
						auto& rb = m_registry->getComponent<RigidBody2D>(e);
						if (rb.type == BodyType::Fixed || rb.isFixed)
						{
							entLabel += " [Fixed]";
						}
						else
						{
							entLabel += " [m=" + std::to_string(rb.mass).substr(0, 4) + "]";
						}
					}

					if (ImGui::Selectable((entLabel + "##c_insp_" + std::to_string(static_cast<uint32_t>(e))).c_str(),
										  false))
					{
						m_selectedEntity = e;
						m_selectedClusterId = 0;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Click to inspect and edit components for this entity.");
					}
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("Destroy Cluster", ImVec2(-1.0f, 26.0f)))
			{
				deleteSelected();
			}
		}

		void renderEntityInspector()
		{
			Entity e = m_selectedEntity;
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Entity #%u", e.id);

			auto* parentCluster = m_services->clusters().getClusterForEntity(e);
			if (parentCluster)
			{
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Belongs to Cluster: %s",
								   parentCluster->name.c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("Select Cluster##from_ent"))
				{
					m_selectedClusterId = parentCluster->id;
					m_selectedEntity = INVALID_ENTITY;
					return;
				}

				for (const auto& [localName, ent] : parentCluster->localNames)
				{
					if (ent == e)
					{
						ImGui::TextDisabled("Local Name: '%s'", localName.c_str());
						break;
					}
				}
			}
			ImGui::Separator();

			// 1. Identity: Unique Name
			std::string currentName = m_services->names().getName(e);
			std::snprintf(m_entityNameBuf, sizeof(m_entityNameBuf), "%s", currentName.c_str());
			if (ImGui::InputText("Unique Name", m_entityNameBuf, sizeof(m_entityNameBuf)))
			{
				if (m_entityNameBuf[0] == '\0')
				{
					m_services->names().removeName(e);
				}
				else
				{
					m_services->names().setName(e, m_entityNameBuf);
				}
				m_isSceneDirty = true;
			}

			// 2. Identity: Tags
			ImGui::Spacing();
			ImGui::Text("Tags:");
			std::vector<std::string> tags = m_services->tags().getTags(e);
			for (const auto& tag : tags)
			{
				ImGui::PushID(tag.c_str());
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.5f, 1.0f), "[%s]", tag.c_str());
				ImGui::SameLine();
				if (ImGui::SmallButton("x"))
				{
					m_services->tags().removeTag(e, tag);
					m_isSceneDirty = true;
				}
				else
				{
					ImGui::SameLine();
				}
				ImGui::PopID();
			}

			ImGui::NewLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::InputTextWithHint("##EntityAddTag", "New tag...", m_newTagBuf, sizeof(m_newTagBuf));
			ImGui::SameLine();
			if (ImGui::Button("Add Tag") && m_newTagBuf[0] != '\0')
			{
				m_services->tags().addTag(e, m_newTagBuf);
				m_newTagBuf[0] = '\0';
				m_isSceneDirty = true;
			}

			ImGui::Separator();

			// 3. Transform
			if (m_registry->hasComponent<Transform>(e))
			{
				if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& t = m_registry->getComponent<Transform>(e);
					float p[3] = {t.position.x, t.position.y, t.position.z};
					if (ImGui::DragFloat3("Position", p, 0.1f))
					{
						vec2 delta(p[0] - t.position.x, p[1] - t.position.y);
						t.position = vec3(p[0], p[1], p[2]);
						m_registry->setComponentDirty(t);
						if (m_registry->hasComponent<Shape>(e))
						{
							auto& s = m_registry->getComponent<Shape>(e);
							offsetShapeParameters(s, delta);
							m_registry->setComponentDirty(s);
						}
						if (m_registry->hasComponent<RigidBody2D>(e))
						{
							m_simulation2D.setPosition(m_registry->getComponent<RigidBody2D>(e).simulationId,
													   vec2(p[0], p[1]));
						}
						m_isSceneDirty = true;
					}

					float r[3] = {t.rotation.x, t.rotation.y, t.rotation.z};
					if (ImGui::DragFloat3("Rotation", r, 0.05f))
					{
						t.rotation = vec3(r[0], r[1], r[2]);
						m_registry->setComponentDirty(t);
						m_isSceneDirty = true;
					}

					float s[3] = {t.scale.x, t.scale.y, t.scale.z};
					if (ImGui::DragFloat3("Scale", s, 0.05f))
					{
						t.scale = vec3(s[0], s[1], s[2]);
						m_registry->setComponentDirty(t);
						m_isSceneDirty = true;
					}

					if (e != m_services->render().getCameraEntity())
					{
						if (ImGui::Button("Remove Transform", ImVec2(-1.0f, 22.0f)))
						{
							m_registry->getComponentManager<Transform>()->removeData(e);
							m_registry->freeRemovedComponents();
							m_isSceneDirty = true;
						}
					}
				}
			}

			// 4. Shape Component
			if (m_registry->hasComponent<Shape>(e))
			{
				if (ImGui::CollapsingHeader("Shape", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& s = m_registry->getComponent<Shape>(e);

					// Shape ID
					int shapeId = static_cast<int>(s.distanceFieldId);
					if (ImGui::InputInt("DistanceField ID", &shapeId))
					{
						s.distanceFieldId = static_cast<uint16_t>(shapeId);
						m_registry->setComponentDirty(s);
						m_2DWorldRenderContext.shapesNeedUpdate = true;
						m_isSceneDirty = true;
					}

					// CSG Combination
					const char* combos[] = {"Addition", "Subtraction", "Intersection", "SmoothAddition",
											"SmoothSubtraction"};
					int combIdx = static_cast<int>(s.combination);
					if (ImGui::Combo("CSG Mode", &combIdx, combos, IM_ARRAYSIZE(combos)))
					{
						s.combination = static_cast<CombinationType>(combIdx);
						m_registry->setComponentDirty(s);
						m_2DWorldRenderContext.shapesNeedUpdate = true;
						m_isSceneDirty = true;
					}

					if (combIdx >= 3)
					{
						if (ImGui::SliderFloat("Smooth Factor", &s.smoothFactor, 0.01f, 10.0f))
						{
							m_registry->setComponentDirty(s);
							m_2DWorldRenderContext.shapesNeedUpdate = true;
							m_isSceneDirty = true;
						}
					}

					// Material ID
					if (renderMaterialSelector("Material Slot", s.material))
					{
						m_registry->setComponentDirty(s);
						m_2DWorldRenderContext.shapesNeedUpdate = true;
						m_isSceneDirty = true;
					}

					// Collision
					if (ImGui::Checkbox("Collisions Enabled", &s.hasCollisions))
					{
						m_registry->setComponentDirty(s);
						m_isSceneDirty = true;
					}

					// Parameters 0..7
					ImGui::Text("Parameters:");
					const auto* customData = m_services->shapes().getCustomShapeData(s.distanceFieldId);
					if (customData)
					{
						ImGui::SameLine();
						if (ImGui::SmallButton("Reset Defaults"))
						{
							std::copy(customData->defaultParameters.begin(), customData->defaultParameters.end(),
									  s.parameters);
							m_registry->setComponentDirty(s);
							m_2DWorldRenderContext.shapesNeedUpdate = true;
							m_isSceneDirty = true;
						}
						if (!customData->filePath.empty())
						{
							float avail = ImGui::GetContentRegionAvail().x;
							if (ImGui::Button("Edit in Shape Editor", ImVec2(avail * 0.65f - 4.0f, 22.0f)))
							{
								launchShapeEditor(customData->filePath);
							}
							ImGui::SameLine();
							if (ImGui::Button("Reload Shape", ImVec2(-1.0f, 22.0f)))
							{
								reloadCustomShape(customData->filePath);
							}
						}
					}

					for (int p = 0; p < 8; ++p)
					{
						std::string paramLabel = "Param " + std::to_string(p);
						if (s.distanceFieldId == DefaultShapes::Circle)
						{
							if (p == DefaultShapes::Circle::PosX)
								paramLabel = "Pos X";
							else if (p == DefaultShapes::Circle::PosY)
								paramLabel = "Pos Y";
							else if (p == DefaultShapes::Circle::Radius)
								paramLabel = "Radius";
						}
						else if (s.distanceFieldId == DefaultShapes::Box)
						{
							if (p == DefaultShapes::Box::PosX)
								paramLabel = "Pos X";
							else if (p == DefaultShapes::Box::PosY)
								paramLabel = "Pos Y";
							else if (p == DefaultShapes::Box::SizeX)
								paramLabel = "Size X";
							else if (p == DefaultShapes::Box::SizeY)
								paramLabel = "Size Y";
						}
						else if (s.distanceFieldId == DefaultShapes::RoundBox)
						{
							if (p == DefaultShapes::RoundBox::PosX)
								paramLabel = "Pos X";
							else if (p == DefaultShapes::RoundBox::PosY)
								paramLabel = "Pos Y";
							else if (p == DefaultShapes::RoundBox::SizeX)
								paramLabel = "Size X";
							else if (p == DefaultShapes::RoundBox::SizeY)
								paramLabel = "Size Y";
							else if (p == DefaultShapes::RoundBox::Radius)
								paramLabel = "Radius";
						}
						else if (s.distanceFieldId == DefaultShapes::Triangle)
						{
							if (p == DefaultShapes::Triangle::PosX)
								paramLabel = "Pos X";
							else if (p == DefaultShapes::Triangle::PosY)
								paramLabel = "Pos Y";
							else if (p == DefaultShapes::Triangle::Width)
								paramLabel = "Width";
							else if (p == DefaultShapes::Triangle::Height)
								paramLabel = "Height";
						}
						else if (s.distanceFieldId == DefaultShapes::Line)
						{
							if (p == DefaultShapes::Line::StartX)
								paramLabel = "Start X";
							else if (p == DefaultShapes::Line::StartY)
								paramLabel = "Start Y";
							else if (p == DefaultShapes::Line::EndX)
								paramLabel = "End X";
							else if (p == DefaultShapes::Line::EndY)
								paramLabel = "End Y";
							else if (p == DefaultShapes::Line::Width)
								paramLabel = "Width";
						}
						else if (s.distanceFieldId == DefaultShapes::Ramp)
						{
							if (p == DefaultShapes::Ramp::PosX)
								paramLabel = "Pos X";
							else if (p == DefaultShapes::Ramp::PosY)
								paramLabel = "Pos Y";
							else if (p == DefaultShapes::Ramp::Width)
								paramLabel = "Width";
							else if (p == DefaultShapes::Ramp::Height)
								paramLabel = "Height";
							else if (p == DefaultShapes::Ramp::Skew)
								paramLabel = "Skew";
						}
						else if (s.distanceFieldId == DefaultShapes::Star)
						{
							if (p == 0)
								paramLabel = "Pos X";
							else if (p == 1)
								paramLabel = "Pos Y";
							else if (p == 2)
								paramLabel = "Radius";
							else if (p == 3)
								paramLabel = "Displace";
							else if (p == 4)
								paramLabel = "Points";
							else if (p == 5)
								paramLabel = "Speed";
						}
						else if (customData && !customData->parameterNames[p].empty())
						{
							paramLabel = customData->parameterNames[p];
						}

						float oldVal = s.parameters[p];
						if (ImGui::DragFloat(paramLabel.c_str(), &s.parameters[p], 0.05f))
						{
							float delta = s.parameters[p] - oldVal;
							if (m_registry->hasComponent<Transform>(e))
							{
								auto& t = m_registry->getComponent<Transform>(e);
								if (p == 0 && s.distanceFieldId != DefaultShapes::Line)
								{
									t.position.x += delta;
									m_registry->setComponentDirty(t);
								}
								else if (p == 1 && s.distanceFieldId != DefaultShapes::Line)
								{
									t.position.y += delta;
									m_registry->setComponentDirty(t);
								}
							}
							m_registry->setComponentDirty(s);
							m_isSceneDirty = true;
						}
					}

					if (ImGui::Button("Remove Shape", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<Shape>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_2DWorldRenderContext.shapesNeedUpdate = true;
						m_isSceneDirty = true;
					}
				}
			}

			// 5. RigidBody2D Component
			if (m_registry->hasComponent<RigidBody2D>(e))
			{
				if (ImGui::CollapsingHeader("RigidBody2D", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& rb = m_registry->getComponent<RigidBody2D>(e);

					const char* bodyTypes[] = {"Dynamic", "Kinematic", "Fixed"};
					int btype = static_cast<int>(rb.type);
					if (ImGui::Combo("Body Type", &btype, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
					{
						rb.type = static_cast<BodyType>(btype);
						rb.isFixed = (rb.type == BodyType::Fixed);
						m_registry->setComponentDirty(rb);
						m_simulation2D.setBodyType(rb.simulationId, rb.type);
						m_isSceneDirty = true;
					}

					if (ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f))
					{
						m_registry->setComponentDirty(rb);
						m_simulation2D.setMass(rb.simulationId, rb.mass);
						m_isSceneDirty = true;
					}

					// Quick fix/unfix toggle
					if (rb.type != BodyType::Fixed)
					{
						if (ImGui::Button("Fix in Place (Fixed)", ImVec2(-1.0f, 22.0f)))
						{
							rb.type = BodyType::Fixed;
							rb.isFixed = true;
							m_registry->setComponentDirty(rb);
							m_simulation2D.setBodyType(rb.simulationId, BodyType::Fixed);
							m_isSceneDirty = true;
						}
					}
					else
					{
						if (ImGui::Button("Unfix (Dynamic)", ImVec2(-1.0f, 22.0f)))
						{
							rb.type = BodyType::Dynamic;
							rb.isFixed = false;
							m_registry->setComponentDirty(rb);
							m_simulation2D.setBodyType(rb.simulationId, BodyType::Dynamic);
							m_isSceneDirty = true;
						}
					}

					if (ImGui::Checkbox("Collision", &rb.enableCollision))
					{
						m_registry->setComponentDirty(rb);
						m_simulation2D.setCollisionEnabled(rb.simulationId, rb.enableCollision);
						m_isSceneDirty = true;
					}

					if (ImGui::Checkbox("Shape Collision", &rb.enableShapeCollision))
					{
						m_registry->setComponentDirty(rb);
						m_simulation2D.setShapeCollisionEnabled(rb.simulationId, rb.enableShapeCollision);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove RigidBody", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<RigidBody2D>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 6. Dot Component
			if (m_registry->hasComponent<Dot>(e))
			{
				if (ImGui::CollapsingHeader("Dot", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& dot = m_registry->getComponent<Dot>(e);
					if (ImGui::Checkbox("Static Dot", &dot.isStatic))
					{
						m_registry->setComponentDirty(dot);
						m_isSceneDirty = true;
					}

					if (renderMaterialSelector("Dot Material", dot.materialId))
					{
						m_registry->setComponentDirty(dot);
						m_2DWorldRenderContext.shapesNeedUpdate = true;
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Dot", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<Dot>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 7. Spring Component
			if (m_registry->hasComponent<Spring>(e))
			{
				if (ImGui::CollapsingHeader("Spring", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& spring = m_registry->getComponent<Spring>(e);

					int entA = (spring.entityA == INVALID_ENTITY) ? -1 : static_cast<int>(spring.entityA);
					int entB = (spring.entityB == INVALID_ENTITY) ? -1 : static_cast<int>(spring.entityB);

					std::string nameA = (spring.entityA != INVALID_ENTITY && m_registry->isEntityAlive(spring.entityA))
											? formatEntityLabel(spring.entityA)
											: "None";
					std::string nameB = (spring.entityB != INVALID_ENTITY && m_registry->isEntityAlive(spring.entityB))
											? formatEntityLabel(spring.entityB)
											: "None";

					ImGui::Text("Connected Entities:");
					if (ImGui::InputInt("Entity A", &entA))
					{
						spring.entityA = (entA < 0) ? INVALID_ENTITY : static_cast<Entity>(entA);
						m_registry->setComponentDirty(spring);
						syncConstraintClusterMembership(e, spring.entityA, spring.entityB);
						m_isSceneDirty = true;
					}
					ImGui::TextDisabled("A: %s", nameA.c_str());

					if (ImGui::InputInt("Entity B", &entB))
					{
						spring.entityB = (entB < 0) ? INVALID_ENTITY : static_cast<Entity>(entB);
						m_registry->setComponentDirty(spring);
						syncConstraintClusterMembership(e, spring.entityA, spring.entityB);
						m_isSceneDirty = true;
					}
					ImGui::TextDisabled("B: %s", nameB.c_str());

					if (spring.entityA != INVALID_ENTITY && spring.entityB != INVALID_ENTITY &&
						m_registry->isEntityAlive(spring.entityA) && m_registry->isEntityAlive(spring.entityB))
					{
						vec2 posA = getEntityPosition(spring.entityA);
						vec2 posB = getEntityPosition(spring.entityB);
						float curDist = glm::distance(posA, posB);
						ImGui::Text("Current Distance: %.2f", curDist);
						ImGui::SameLine();
						if (ImGui::SmallButton("Set Rest to Current"))
						{
							spring.restDistance = curDist;
							m_registry->setComponentDirty(spring);
							m_isSceneDirty = true;
						}
					}

					if (ImGui::DragFloat("Stiffness", &spring.stiffness, 0.05f, 0.0f, 1000.0f))
					{
						m_registry->setComponentDirty(spring);
						m_isSceneDirty = true;
					}

					if (ImGui::DragFloat("Rest Distance", &spring.restDistance, 0.05f, 0.0f, 1000.0f))
					{
						m_registry->setComponentDirty(spring);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Spring", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<Spring>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 8. DistanceConstraint Component
			if (m_registry->hasComponent<DistanceConstraint>(e))
			{
				if (ImGui::CollapsingHeader("Distance Constraint", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& dc = m_registry->getComponent<DistanceConstraint>(e);

					int entA = (dc.entityA == INVALID_ENTITY) ? -1 : static_cast<int>(dc.entityA);
					int entB = (dc.entityB == INVALID_ENTITY) ? -1 : static_cast<int>(dc.entityB);

					std::string nameA = (dc.entityA != INVALID_ENTITY && m_registry->isEntityAlive(dc.entityA))
											? formatEntityLabel(dc.entityA)
											: "None";
					std::string nameB = (dc.entityB != INVALID_ENTITY && m_registry->isEntityAlive(dc.entityB))
											? formatEntityLabel(dc.entityB)
											: "None";

					ImGui::Text("Connected Entities:");
					if (ImGui::InputInt("Entity A", &entA))
					{
						dc.entityA = (entA < 0) ? INVALID_ENTITY : static_cast<Entity>(entA);
						m_registry->setComponentDirty(dc);
						syncConstraintClusterMembership(e, dc.entityA, dc.entityB);
						m_isSceneDirty = true;
					}
					ImGui::TextDisabled("A: %s", nameA.c_str());

					if (ImGui::InputInt("Entity B", &entB))
					{
						dc.entityB = (entB < 0) ? INVALID_ENTITY : static_cast<Entity>(entB);
						m_registry->setComponentDirty(dc);
						syncConstraintClusterMembership(e, dc.entityA, dc.entityB);
						m_isSceneDirty = true;
					}
					ImGui::TextDisabled("B: %s", nameB.c_str());

					if (dc.entityA != INVALID_ENTITY && dc.entityB != INVALID_ENTITY &&
						m_registry->isEntityAlive(dc.entityA) && m_registry->isEntityAlive(dc.entityB))
					{
						vec2 posA = getEntityPosition(dc.entityA);
						vec2 posB = getEntityPosition(dc.entityB);
						float curDist = glm::distance(posA, posB);
						ImGui::Text("Current Distance: %.2f", curDist);
						ImGui::SameLine();
						if (ImGui::SmallButton("Set Distance to Current"))
						{
							dc.distance = curDist;
							m_registry->setComponentDirty(dc);
							m_isSceneDirty = true;
						}
					}

					if (ImGui::DragFloat("Target Distance", &dc.distance, 0.05f, 0.0f, 1000.0f))
					{
						m_registry->setComponentDirty(dc);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Distance Constraint", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<DistanceConstraint>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 9. Light2D Component
			if (m_registry->hasComponent<Light2DComponent>(e))
			{
				if (ImGui::CollapsingHeader("Light 2D", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& light = m_registry->getComponent<Light2DComponent>(e);

					const char* lightTypes[] = {"Directional", "Point", "Cone"};
					int typeIdx = static_cast<int>(light.type);
					if (ImGui::Combo("Light Type", &typeIdx, lightTypes, IM_ARRAYSIZE(lightTypes)))
					{
						light.type = static_cast<Light2DType>(typeIdx);
						m_registry->setComponentDirty(light);
						m_isSceneDirty = true;
					}

					float col[4] = {light.color.r, light.color.g, light.color.b, light.color.a};
					if (ImGui::ColorEdit4("Color & Intensity", col))
					{
						light.color = glm::vec4(col[0], col[1], col[2], col[3]);
						m_registry->setComponentDirty(light);
						m_isSceneDirty = true;
					}

					if (light.type == Light2DType::Directional || light.type == Light2DType::Cone)
					{
						float dir[2] = {light.direction.x, light.direction.y};
						if (ImGui::DragFloat2("Direction", dir, 0.05f))
						{
							if (glm::length(glm::vec2(dir[0], dir[1])) > 0.001f)
								light.direction = glm::normalize(glm::vec2(dir[0], dir[1]));
							m_registry->setComponentDirty(light);
							m_isSceneDirty = true;
						}
					}

					if (light.type == Light2DType::Point || light.type == Light2DType::Cone)
					{
						if (ImGui::DragFloat("Radius", &light.radius, 0.5f, 0.1f, 1000.0f))
						{
							m_registry->setComponentDirty(light);
							m_isSceneDirty = true;
						}
					}

					if (light.type == Light2DType::Cone)
					{
						if (ImGui::SliderFloat("Cone Angle", &light.coneAngle, 1.0f, 180.0f))
						{
							m_registry->setComponentDirty(light);
							m_isSceneDirty = true;
						}
						if (ImGui::SliderFloat("Cone Penumbra", &light.conePenumbra, 0.0f, 90.0f))
						{
							m_registry->setComponentDirty(light);
							m_isSceneDirty = true;
						}
					}

					if (ImGui::Checkbox("Cast Shadows", &light.castShadows))
					{
						m_registry->setComponentDirty(light);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Light 2D", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<Light2DComponent>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 10. Camera Component
			if (m_registry->hasComponent<ECS::Camera>(e))
			{
				if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& cam = m_registry->getComponent<ECS::Camera>(e);

					if (ImGui::DragFloat("Near Plane", &cam.nearPlane, 0.05f, 0.001f, 100.0f))
					{
						m_registry->setComponentDirty(cam);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Far Plane", &cam.farPlane, 1.0f, 1.0f, 10000.0f))
					{
						m_registry->setComponentDirty(cam);
						m_isSceneDirty = true;
					}

					if (e != m_services->render().getCameraEntity())
					{
						if (ImGui::Button("Remove Camera", ImVec2(-1.0f, 22.0f)))
						{
							m_registry->getComponentManager<ECS::Camera>()->removeData(e);
							m_registry->freeRemovedComponents();
							m_isSceneDirty = true;
						}
					}
					else
					{
						ImGui::TextDisabled("(Active Main Viewport Camera)");
					}
				}
			}

			// 11. TextRenderer Component
			if (m_registry->hasComponent<TextRenderer>(e))
			{
				if (ImGui::CollapsingHeader("Text Renderer", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& tr = m_registry->getComponent<TextRenderer>(e);

					char textBuf[33];
					std::snprintf(textBuf, sizeof(textBuf), "%s", tr.text.c_str());
					if (ImGui::InputText("Text", textBuf, sizeof(textBuf)))
					{
						tr.text = textBuf;
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					if (renderMaterialSelector("Text Material", tr.material))
					{
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					const char* hAligns[] = {"Left", "Center", "Right"};
					int hIdx = static_cast<int>(tr.horizontalAlignment);
					if (ImGui::Combo("Horizontal Align", &hIdx, hAligns, IM_ARRAYSIZE(hAligns)))
					{
						tr.horizontalAlignment = static_cast<TextRenderer::HorizontalAlignment>(hIdx);
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					const char* vAligns[] = {"Top", "Center", "Bottom"};
					int vIdx = static_cast<int>(tr.verticalAlignment);
					if (ImGui::Combo("Vertical Align", &vIdx, vAligns, IM_ARRAYSIZE(vAligns)))
					{
						tr.verticalAlignment = static_cast<TextRenderer::VerticalAlignment>(vIdx);
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Text Renderer", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<TextRenderer>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			if (m_registry->hasComponent<UITextRenderer>(e))
			{
				if (ImGui::CollapsingHeader("UI Text Renderer", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& tr = m_registry->getComponent<UITextRenderer>(e);

					char textBuf[33];
					std::snprintf(textBuf, sizeof(textBuf), "%s", tr.text.c_str());
					if (ImGui::InputText("Text##UI", textBuf, sizeof(textBuf)))
					{
						tr.text = textBuf;
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					if (renderMaterialSelector("Text Material##UI", tr.material))
					{
						m_registry->setComponentDirty(tr);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove UI Text Renderer", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<UITextRenderer>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 12. FlyMovement Components
			if (m_registry->hasComponent<ECS::FlyMovement2D>(e))
			{
				if (ImGui::CollapsingHeader("Fly Movement 2D", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& fly = m_registry->getComponent<ECS::FlyMovement2D>(e);
					if (ImGui::DragFloat("Speed", &fly.speed, 0.1f, 0.0f, 100.0f))
					{
						m_registry->setComponentDirty(fly);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Scroll Speed", &fly.scrollSpeed, 0.1f, 0.0f, 100.0f))
					{
						m_registry->setComponentDirty(fly);
						m_isSceneDirty = true;
					}
					if (ImGui::Checkbox("Smooth", &fly.isSmooth))
					{
						m_registry->setComponentDirty(fly);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Fly Movement 2D", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<ECS::FlyMovement2D>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			if (m_registry->hasComponent<ECS::FlyMovement>(e))
			{
				if (ImGui::CollapsingHeader("Fly Movement (3D)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& fly = m_registry->getComponent<ECS::FlyMovement>(e);
					if (ImGui::DragFloat("Speed", &fly.speed, 0.1f, 0.0f, 100.0f))
					{
						m_registry->setComponentDirty(fly);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Sensitivity", &fly.sensitivity, 0.01f, 0.0f, 10.0f))
					{
						m_registry->setComponentDirty(fly);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove Fly Movement", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<ECS::FlyMovement>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 13. Button Components
			if (m_registry->hasComponent<ShapeButton>(e))
			{
				if (ImGui::CollapsingHeader("Shape Button", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& btn = m_registry->getComponent<ShapeButton>(e);
					if (ImGui::DragFloat("Modifier Amount", &btn.modifierAmount, 0.05f))
					{
						m_registry->setComponentDirty(btn);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Click Padding", &btn.clickPadding, 0.5f, 0.0f, 100.0f))
					{
						m_registry->setComponentDirty(btn);
						m_isSceneDirty = true;
					}
					ImGui::Text("Hovered: %s", btn.hovered ? "True" : "False");

					if (ImGui::Button("Remove Shape Button", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<ShapeButton>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			if (m_registry->hasComponent<ShapeToggle>(e))
			{
				if (ImGui::CollapsingHeader("Shape Toggle", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& tog = m_registry->getComponent<ShapeToggle>(e);
					if (ImGui::Checkbox("Active", &tog.active))
					{
						m_registry->setComponentDirty(tog);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Modifier Amount", &tog.modifierAmount, 0.05f))
					{
						m_registry->setComponentDirty(tog);
						m_isSceneDirty = true;
					}
					if (ImGui::DragFloat("Click Padding", &tog.clickPadding, 0.5f, 0.0f, 100.0f))
					{
						m_registry->setComponentDirty(tog);
						m_isSceneDirty = true;
					}
					ImGui::Text("Hovered: %s", tog.hovered ? "True" : "False");

					if (ImGui::Button("Remove Shape Toggle", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentManager<ShapeToggle>()->removeData(e);
						m_registry->freeRemovedComponents();
						m_isSceneDirty = true;
					}
				}
			}

			// 14. Attached Scripts and Custom Components
			auto allCompTypes = m_registry->getComponentTypes(e);
			for (size_t compID : allCompTypes)
			{
				std::string compName = m_registry->getComponentName(compID);

				if (compName == "Transform" || compName == "Shape" || compName == "RigidBody2D" || compName == "Dot" ||
					compName == "UIDot" || compName == "UIShape" || compName == "Spring" ||
					compName == "DistanceConstraint" || compName == "Light2DComponent" || compName == "TextRenderer" ||
					compName == "UITextRenderer" || compName == "Camera" || compName == "ShapeButton" ||
					compName == "ShapeToggle" || compName == "FlyMovement" || compName == "FlyMovement2D")
				{
					continue;
				}

				std::string headerTitle = (compName == "Unknown")
											  ? ("Missing Script (Type ID: " + std::to_string(compID) + ")")
											  : (compName + " (Script)");

				ImGui::PushID(static_cast<int>(compID));
				if (ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f),
									   compName == "Unknown" ? "Missing script or unregistered component type."
															 : "Custom component / script attached.");
					ImGui::BulletText("Component Name: %s", compName.c_str());
					ImGui::BulletText("Type ID: %zu", compID);
				}
				ImGui::PopID();
			}

			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("Delete Entity", ImVec2(-1.0f, 26.0f)))
			{
				deleteSelected();
			}
		}

		// =====================================================================
		// UI: Bottom Toolbar (Cluster Mode & Sim Controls)
		// =====================================================================
		void renderBottomToolbar()
		{
			const float winW = ImGui::GetIO().DisplaySize.x;
			const float winH = ImGui::GetIO().DisplaySize.y;
			const float bottomW = winW - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH;

			// Positioned flush between left panel and right panel at the very bottom
			ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH, winH - BOTTOM_BAR_HEIGHT), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(bottomW, BOTTOM_BAR_HEIGHT), ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 6.0f));

			if (ImGui::Begin("##BottomToolbar", nullptr, flags))
			{
				// Cluster selection / move mode toggle
				if (m_selectIndividualClusterEntities)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.1f, 1.0f));
					if (ImGui::Button("Move: Rigidbody (C)", ImVec2(190.0f, 26.0f)))
					{
						m_selectIndividualClusterEntities = false;
					}
					ImGui::PopStyleColor();
				}
				else
				{
					if (ImGui::Button("Move: Entire Cluster (C)", ImVec2(190.0f, 26.0f)))
					{
						m_selectIndividualClusterEntities = true;
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Toggle whether clicking a cluster moves the whole cluster or selects/moves an "
									  "individual rigidbody.\nShortcut: Press 'C' or hold 'Alt' while clicking.");
				}

				// Simulation Play/Pause button
				ImGui::SameLine(bottomW - 140.0f);
				if (m_isSimulating)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.2f, 1.0f));
					if (ImGui::Button("Pause (Space)", ImVec2(120.0f, 26.0f)))
					{
						toggleSimulation();
					}
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
					if (ImGui::Button("Play (Space)", ImVec2(120.0f, 26.0f)))
					{
						toggleSimulation();
					}
					ImGui::PopStyleColor();
				}
			}
			ImGui::End();
			ImGui::PopStyleVar(3);
			ImGui::PopStyleColor();
		}

		// =====================================================================
		// UI: Viewport Creation Context Menu
		// =====================================================================
		void renderViewportContextMenu()
		{
			if (m_openViewportContextMenu)
			{
				ImGui::OpenPopup("ViewportCreationContextMenu");
				m_openViewportContextMenu = false;
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
			if (ImGui::BeginPopup("ViewportCreationContextMenu"))
			{
				vec2 spawnPos = m_contextMenuWorldPos;
				if (m_gridSnap && m_gridSize > 0.001f)
				{
					spawnPos.x = std::round(spawnPos.x / m_gridSize) * m_gridSize;
					spawnPos.y = std::round(spawnPos.y / m_gridSize) * m_gridSize;
				}

				ImGui::TextDisabled("Create at (%.2f, %.2f)", spawnPos.x, spawnPos.y);
				ImGui::Separator();

				// 1. Primitives
				if (ImGui::BeginMenu("Primitives"))
				{
					if (ImGui::MenuItem("Circle"))
					{
						spawnDefaultShape(DefaultShapes::Circle, spawnPos);
					}
					if (ImGui::MenuItem("Box"))
					{
						spawnDefaultShape(DefaultShapes::Box, spawnPos);
					}
					if (ImGui::MenuItem("RoundBox"))
					{
						spawnDefaultShape(DefaultShapes::RoundBox, spawnPos);
					}
					if (ImGui::MenuItem("Triangle"))
					{
						spawnDefaultShape(DefaultShapes::Triangle, spawnPos);
					}
					if (ImGui::MenuItem("Line"))
					{
						spawnDefaultShape(DefaultShapes::Line, spawnPos);
					}
					if (ImGui::MenuItem("Ramp"))
					{
						spawnDefaultShape(DefaultShapes::Ramp, spawnPos);
					}
					if (ImGui::MenuItem("Star"))
					{
						spawnDefaultShape(DefaultShapes::Star, spawnPos);
					}
					ImGui::EndMenu();
				}

				// 2. Clusters (From Assets)
				const auto& clusterAssets = m_project.getClusters();
				if (ImGui::BeginMenu("Clusters"))
				{
					if (clusterAssets.empty())
					{
						ImGui::TextDisabled("No cluster assets in project");
					}
					else
					{
						for (const auto& ca : clusterAssets)
						{
							const char* label =
								!ca.relativePath.empty() ? ca.relativePath.c_str() : ca.fileName.c_str();
							if (ImGui::MenuItem(label))
							{
								spawnCluster(ca.fullPath.string(), spawnPos);
							}
						}
					}
					ImGui::EndMenu();
				}

				// 3. Custom Shapes (From Assets)
				const auto& shapeAssets = m_project.getCustomShapes();
				if (ImGui::BeginMenu("Custom Shapes"))
				{
					if (shapeAssets.empty())
					{
						ImGui::TextDisabled("No custom shape assets in project");
					}
					else
					{
						for (const auto& sa : shapeAssets)
						{
							const char* label =
								!sa.relativePath.empty() ? sa.relativePath.c_str() : sa.fileName.c_str();
							if (ImGui::MenuItem(label))
							{
								spawnCustomShape(sa.fullPath.string(), spawnPos);
							}
						}
					}
					ImGui::EndMenu();
				}

				ImGui::EndPopup();
			}
			ImGui::PopStyleVar(2);
		}

		// =====================================================================
		// UI: Project Welcome Modal
		// =====================================================================
		void renderProjectModal()
		{
			if (m_project.isOpen())
			{
				return;
			}

			const float winW = ImGui::GetIO().DisplaySize.x;
			const float winH = ImGui::GetIO().DisplaySize.y;

			ImGui::SetNextWindowSize(ImVec2(450.0f, 280.0f), ImGuiCond_Always);
			ImGui::SetNextWindowPos(ImVec2(winW * 0.5f - 225.0f, winH * 0.5f - 140.0f), ImGuiCond_Always);

			if (ImGui::Begin("Welcome to Scene Editor", nullptr,
							 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
			{
				ImGui::TextWrapped("Select a project folder containing an 'assets/' directory to begin editing.");
				ImGui::Spacing();

				if (ImGui::Button("Open Project Folder...", ImVec2(-1.0f, 32.0f)))
				{
					m_project.openFolderDialog();
				}

				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Text("Recent Projects:");

				if (m_project.getRecentProjects().empty())
				{
					ImGui::TextDisabled("No recent projects found.");
				}
				else
				{
					for (const auto& p : m_project.getRecentProjects())
					{
						if (ImGui::Selectable(p.c_str()))
						{
							m_project.open(p);
						}
					}
				}

				ImGui::End();
			}
		}
	};
} // namespace WeirdEngine
