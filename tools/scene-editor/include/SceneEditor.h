#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include <imgui.h>
#include <SDL3/SDL_dialog.h>

#include "EditorProject.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/SceneSerializer.h"
#include "weird-engine/services/ServiceProvider.h"
#include "weird-engine/shapes/NodeGraph.h"
#include "weird-engine/shapes/NodeGraphCompiler.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/Shape.h"
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

		// Selection
		Entity m_selectedEntity = INVALID_ENTITY;
		uint32_t m_selectedClusterId = 0;

		// Drag state
		bool m_isDragging = false;
		vec2 m_dragStartMouseWorld{0.0f, 0.0f};
		vec2 m_dragStartPos{0.0f, 0.0f};

		// Camera control state
		bool m_middleWasDown = false;
		bool m_rightWasDown = false;
		vec2 m_rightDragStartScreen{0.0f, 0.0f};

		// Grid & Snapping
		bool m_gridEnabled = true;
		bool m_gridSnap = false;
		float m_gridSize = 1.0f;

		// Simulation state
		bool m_isSimulating = false;
		bool m_hasSnapshot = false;

		// UI Panel Layout
		float m_menuBarHeight = 26.0f;
		static constexpr float LEFT_PANEL_WIDTH = 320.0f;
		static constexpr float RIGHT_PANEL_WIDTH = 340.0f;
		static constexpr float BOTTOM_BAR_HEIGHT = 68.0f;

		// Asset Browser Filter
		int m_assetFilterType = 0; // 0 = All, 1 = Scenes, 2 = Clusters, 3 = Shapes
		char m_assetSearchBuf[64] = "";
		char m_hierarchySearchBuf[64] = "";

		// Inspector text buffers
		char m_entityNameBuf[64] = "";
		char m_clusterNameBuf[64] = "";
		char m_newTagBuf[64] = "";

		// File dialog states
		std::mutex m_fileMutex;
		std::string m_pendingLoadScenePath;
		std::string m_pendingSaveScenePath;

		// =====================================================================
		// Lifecycle
		// =====================================================================
		void onStart(Registry& registry, ServiceProvider& services) override
		{
			m_registry = &registry;
			m_services = &services;

			m_services->debug().setDebugInput(true);
			m_services->debug().setDebugFly(false);

			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			cam.position = g_cameraPositon;

			// Initialize default materials if none exist
			for (int i = 0; i < 8; ++i)
			{
				auto& mat = m_services->materials2D().createMaterial("mat_" + std::to_string(i));
				mat.color = ColorPalette::Default[i];
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
		}

		void onUpdate(Registry& registry, ServiceProvider& services) override
		{
			m_registry = &registry;
			m_services = &services;

			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			g_cameraPositon = cam.position;

			m_project.pollDialogs();
			pollFileActions();

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
			if (m_services->input().getKey(Input::LeftCtrl) && m_services->input().getKeyDown(Input::D))
			{
				duplicateSelected();
			}
			if (m_services->input().getKeyDown(Input::Esc))
			{
				deselectAll();
			}
			if (m_services->input().getKeyDown(Input::F) || false)
			{
				resetCamera();
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

			// Right-mouse drag pan
			bool rightDown = m_services->input().getMouseButton(Input::RightClick);
			if (rightDown && m_rightWasDown)
			{
				panCamera(vec2(m_services->input().getMouseDeltaXRaw(), -m_services->input().getMouseDeltaYRaw()));
			}
			m_rightWasDown = rightDown;
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
			const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
			const float scale = newDist / halfHeight;

			cam.position.z = newDist;
			cam.position.x = world.x - (mouse.x - static_cast<float>(Display::width) * 0.5f) * scale;
			cam.position.y = world.y - (mouse.y - halfHeight) * scale;
			m_registry->setComponentDirty(cam);
		}

		void panCamera(vec2 screenDelta)
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			const float halfHeight = (std::max)(1.0f, static_cast<float>(Display::height) * 0.5f);
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
					m_isDragging = true;
					m_dragStartMouseWorld = mouseWorld;
					m_dragStartPos = getSelectedPosition();
				}
			}

			// Left drag: move selected object
			if (m_isDragging && m_services->input().getMouseButton(Input::LeftClick))
			{
				vec2 delta = mouseWorld - m_dragStartMouseWorld;
				vec2 targetPos = m_dragStartPos + delta;
				if (m_gridSnap && m_gridSize > 0.001f)
				{
					targetPos.x = std::round(targetPos.x / m_gridSize) * m_gridSize;
					targetPos.y = std::round(targetPos.y / m_gridSize) * m_gridSize;
				}
				setSelectedPosition(targetPos);
				m_isSceneDirty = true;
			}

			if (m_isDragging && !m_services->input().getMouseButton(Input::LeftClick))
			{
				m_isDragging = false;
			}
		}

		void pickAtWorld(vec2 worldPos)
		{
			// Check clusters first
			for (const auto& cluster : m_services->clusters().getAll())
			{
				for (Entity member : cluster.entities)
				{
					if (m_registry->hasComponent<Transform>(member))
					{
						vec2 memberPos(m_registry->getComponent<Transform>(member).position);
						if (glm::distance(worldPos, memberPos) < 1.0f)
						{
							m_selectedClusterId = cluster.id;
							m_selectedEntity = INVALID_ENTITY;
							return;
						}
					}
				}
			}

			// Check standalone shapes
			auto shapeArray = m_registry->getComponentArray<Shape>();
			if (shapeArray)
			{
				float bestDist = 2.0f;
				Entity bestEntity = INVALID_ENTITY;

				for (size_t i = 0; i < shapeArray->getSize(); ++i)
				{
					Entity e = shapeArray->getEntityAtIdx(i);
					if (m_registry->hasComponent<Transform>(e))
					{
						vec2 pos(m_registry->getComponent<Transform>(e).position);
						float dist = glm::distance(worldPos, pos);
						if (dist < bestDist)
						{
							bestDist = dist;
							bestEntity = e;
						}
					}
				}

				if (bestEntity != INVALID_ENTITY)
				{
					m_selectedEntity = bestEntity;
					m_selectedClusterId = 0;
					return;
				}
			}

			// If clicked empty space, deselect
			deselectAll();
		}

		bool hasSelection() const
		{
			return (m_selectedEntity != INVALID_ENTITY) || (m_selectedClusterId != 0);
		}

		void deselectAll()
		{
			m_selectedEntity = INVALID_ENTITY;
			m_selectedClusterId = 0;
			m_isDragging = false;
		}

		vec2 getSelectedPosition()
		{
			if (m_selectedClusterId != 0)
			{
				auto* c = m_services->clusters().getById(m_selectedClusterId);
				if (c)
				{
					return c->position;
				}
			}
			if (m_selectedEntity != INVALID_ENTITY && m_registry->hasComponent<Transform>(m_selectedEntity))
			{
				return vec2(m_registry->getComponent<Transform>(m_selectedEntity).position);
			}
			return vec2(0.0f);
		}

		void setSelectedPosition(vec2 newPos)
		{
			if (m_selectedClusterId != 0)
			{
				auto* c = m_services->clusters().getById(m_selectedClusterId);
				if (c)
				{
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
							}
						}
					}
				}
			}
			else if (m_selectedEntity != INVALID_ENTITY && m_registry->hasComponent<Transform>(m_selectedEntity))
			{
				auto& t = m_registry->getComponent<Transform>(m_selectedEntity);
				t.position.x = newPos.x;
				t.position.y = newPos.y;
				m_registry->setComponentDirty(t);
				if (m_registry->hasComponent<RigidBody2D>(m_selectedEntity))
				{
					auto& rb = m_registry->getComponent<RigidBody2D>(m_selectedEntity);
					m_simulation2D.setPosition(rb.simulationId, newPos);
				}
			}
		}

		void deleteSelected()
		{
			if (m_selectedClusterId != 0)
			{
				m_services->clusters().destroyCluster(m_selectedClusterId);
				m_selectedClusterId = 0;
				m_isSceneDirty = true;
			}
			else if (m_selectedEntity != INVALID_ENTITY)
			{
				m_services->names().removeName(m_selectedEntity);
				m_services->tags().removeAllTags(m_selectedEntity);
				m_registry->destroyEntity(m_selectedEntity);
				m_selectedEntity = INVALID_ENTITY;
				m_isSceneDirty = true;
			}
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

			// Destroy all entities except camera
			std::vector<Entity> toDestroy;
			auto transformArray = m_registry->getComponentArray<Transform>();
			if (transformArray)
			{
				for (size_t i = 0; i < transformArray->getSize(); ++i)
				{
					Entity e = transformArray->getEntityAtIdx(i);
					if (e != camEntity)
					{
						toDestroy.push_back(e);
					}
				}
			}

			for (Entity e : toDestroy)
			{
				m_services->names().removeName(e);
				m_services->tags().removeAllTags(e);
				m_registry->destroyEntity(e);
			}

			m_clusters.clear();
			m_selectedEntity = INVALID_ENTITY;
			m_selectedClusterId = 0;
			m_currentScenePath.clear();
			m_isSceneDirty = false;
			Logger::log("[SceneEditor] Created new scene.");
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
			m_currentScenePath = path;
			m_isSceneDirty = false;
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
				Logger::log("[SceneEditor] Simulation started (snapshot taken).");
			}
			else
			{
				// Revert to snapshot on pause
				if (m_hasSnapshot)
				{
					m_services->serialization().loadSnapshot("temp_editor.weirdsnap");
					Logger::log("[SceneEditor] Simulation paused (reverted to snapshot).");
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
				s.parameters[DefaultShapes::Circle::Radius] = 1.5f;
			}
			else if (shapeType == DefaultShapes::Box)
			{
				s.parameters[DefaultShapes::Box::SizeX] = 2.0f;
				s.parameters[DefaultShapes::Box::SizeY] = 1.0f;
			}
			else if (shapeType == DefaultShapes::RoundBox)
			{
				s.parameters[DefaultShapes::RoundBox::SizeX] = 2.0f;
				s.parameters[DefaultShapes::RoundBox::SizeY] = 1.0f;
				s.parameters[DefaultShapes::RoundBox::Radius] = 0.3f;
			}
			else if (shapeType == DefaultShapes::Triangle)
			{
				s.parameters[DefaultShapes::Triangle::Width] = 2.0f;
				s.parameters[DefaultShapes::Triangle::Height] = 2.0f;
			}
			else if (shapeType == DefaultShapes::Line)
			{
				s.parameters[DefaultShapes::Line::StartX] = -1.5f;
				s.parameters[DefaultShapes::Line::StartY] = 0.0f;
				s.parameters[DefaultShapes::Line::EndX] = 1.5f;
				s.parameters[DefaultShapes::Line::EndY] = 0.0f;
				s.parameters[DefaultShapes::Line::Width] = 0.15f;
			}
			else if (shapeType == DefaultShapes::Ramp)
			{
				s.parameters[DefaultShapes::Ramp::Width] = 3.0f;
				s.parameters[DefaultShapes::Ramp::Height] = 1.5f;
				s.parameters[DefaultShapes::Ramp::Skew] = 0.5f;
			}

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

			m_selectedEntity = e;
			m_selectedClusterId = 0;
			m_isSceneDirty = true;
			return e;
		}

		void spawnCluster(const std::string& assetPath, vec2 pos)
		{
			auto& cluster = m_services->clusters().instantiate(assetPath, pos);
			m_selectedClusterId = cluster.id;
			m_selectedEntity = INVALID_ENTITY;
			m_isSceneDirty = true;
		}

		vec2 getCameraCenterWorld()
		{
			auto& cam = m_registry->getComponent<Transform>(m_services->render().getCameraEntity());
			return vec2(cam.position.x, cam.position.y);
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
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					if (ImGui::MenuItem("Reset Camera", "F / Home"))
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
					if (ImGui::MenuItem(m_isSimulating ? "Pause Physics" : "Play Physics", "Space"))
					{
						toggleSimulation();
					}
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
			const float winW = static_cast<float>(Display::width);
			const float winH = static_cast<float>(Display::height);
			const float panelH = winH - m_menuBarHeight - BOTTOM_BAR_HEIGHT;

			ImGui::SetNextWindowPos(ImVec2(0.0f, m_menuBarHeight), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(LEFT_PANEL_WIDTH, panelH), ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));

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
					ImGui::EndTabBar();
				}
			}
			ImGui::End();
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
				return nameLower.find(searchStr) != std::string::npos;
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
					for (const auto& clusterAsset : m_project.getClusters())
					{
						if (!matchesSearch(clusterAsset))
							continue;
						ImGui::PushID(clusterAsset.fullPath.string().c_str());
						ImGui::Bullet();
						ImGui::Text("%s", clusterAsset.fileName.c_str());
						ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
						if (ImGui::SmallButton("Spawn"))
						{
							spawnCluster(clusterAsset.fullPath.string(), getCameraCenterWorld());
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
					for (const auto& shapeAsset : m_project.getCustomShapes())
					{
						if (!matchesSearch(shapeAsset))
							continue;
						ImGui::PushID(shapeAsset.fullPath.string().c_str());
						ImGui::Bullet();
						ImGui::Text("%s", shapeAsset.fileName.c_str());
						ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60.0f);
						if (ImGui::SmallButton("Add"))
						{
							spawnCustomShape(shapeAsset.fullPath.string(), getCameraCenterWorld());
						}
						ImGui::PopID();
					}
				}
			}

			ImGui::EndChild();
		}

		void renderHierarchyTab()
		{
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##HierSearch", "Filter entities...", m_hierarchySearchBuf,
									 sizeof(m_hierarchySearchBuf));

			ImGui::BeginChild("##HierarchyList", ImVec2(0.0f, 0.0f), true);

			std::string searchStr(m_hierarchySearchBuf);
			std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

			// 1. Clusters Section
			const auto& clusters = m_services->clusters().getAll();
			if (!clusters.empty())
			{
				if (ImGui::CollapsingHeader("Clusters", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (const auto& cluster : clusters)
					{
						std::string label = cluster.name + " (" + cluster.type + ")##c_" + std::to_string(cluster.id);
						bool isSelected = (m_selectedClusterId == cluster.id);
						if (ImGui::Selectable(label.c_str(), isSelected))
						{
							m_selectedClusterId = cluster.id;
							m_selectedEntity = INVALID_ENTITY;
						}

						// Show local names inside this cluster
						if (ImGui::TreeNode(("Members##cm_" + std::to_string(cluster.id)).c_str()))
						{
							for (const auto& [localName, entity] : cluster.localNames)
							{
								ImGui::BulletText("%s (Entity %u)", localName.c_str(), entity);
							}
							ImGui::TreePop();
						}
					}
				}
			}

			// 2. Standalone Shapes Section
			auto shapeArray = m_registry->getComponentArray<Shape>();
			if (shapeArray && shapeArray->getSize() > 0)
			{
				if (ImGui::CollapsingHeader("Shapes", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (size_t i = 0; i < shapeArray->getSize(); ++i)
					{
						Entity e = shapeArray->getEntityAtIdx(i);

						// Skip entities that belong to a cluster
						if (m_services->clusters().getClusterForEntity(e) != nullptr)
						{
							continue;
						}

						std::string entityName = m_services->names().getName(e);
						std::string display = entityName.empty() ? ("Entity " + std::to_string(e))
																 : (entityName + " (#" + std::to_string(e) + ")");

						if (!searchStr.empty())
						{
							std::string dLower = display;
							std::transform(dLower.begin(), dLower.end(), dLower.begin(), ::tolower);
							if (dLower.find(searchStr) == std::string::npos)
								continue;
						}

						bool isSelected = (m_selectedEntity == e);
						if (ImGui::Selectable(display.c_str(), isSelected))
						{
							m_selectedEntity = e;
							m_selectedClusterId = 0;
						}
					}
				}
			}

			ImGui::EndChild();
		}

		// =====================================================================
		// UI: Right Panel (Inspector)
		// =====================================================================
		void renderInspector()
		{
			const float winW = static_cast<float>(Display::width);
			const float winH = static_cast<float>(Display::height);
			const float panelH = winH - m_menuBarHeight - BOTTOM_BAR_HEIGHT;

			ImGui::SetNextWindowPos(ImVec2(winW - RIGHT_PANEL_WIDTH, m_menuBarHeight), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(RIGHT_PANEL_WIDTH, panelH), ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));

			if (ImGui::Begin("##InspectorPanel", nullptr, flags))
			{
				ImGui::TextDisabled("INSPECTOR");
				ImGui::Separator();

				if (m_selectedClusterId != 0)
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
			ImGui::PopStyleColor();
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
			ImGui::Separator();

			// Cluster Name
			strncpy_s(m_clusterNameBuf, cluster->name.c_str(), sizeof(m_clusterNameBuf) - 1);
			if (ImGui::InputText("Name", m_clusterNameBuf, sizeof(m_clusterNameBuf)))
			{
				cluster->name = m_clusterNameBuf;
				m_isSceneDirty = true;
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

			// Member Entities & Local Names
			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Local Names", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (const auto& [localName, entity] : cluster->localNames)
				{
					ImGui::BulletText("%s -> Entity %u", localName.c_str(), entity);
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
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Entity #%u", e);
			ImGui::Separator();

			// 1. Identity: Unique Name
			std::string currentName = m_services->names().getName(e);
			strncpy_s(m_entityNameBuf, currentName.c_str(), sizeof(m_entityNameBuf) - 1);
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
						t.position = vec3(p[0], p[1], p[2]);
						m_registry->setComponentDirty(t);
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
						m_isSceneDirty = true;
					}

					if (combIdx >= 3)
					{
						if (ImGui::SliderFloat("Smooth Factor", &s.smoothFactor, 0.01f, 10.0f))
						{
							m_registry->setComponentDirty(s);
							m_isSceneDirty = true;
						}
					}

					// Material ID
					int matId = static_cast<int>(s.material);
					if (ImGui::SliderInt("Material Slot", &matId, 0, 15))
					{
						s.material = static_cast<uint16_t>(matId);
						m_registry->setComponentDirty(s);
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
					for (int p = 0; p < 8; ++p)
					{
						std::string paramLabel = "Param " + std::to_string(p);
						if (s.distanceFieldId == DefaultShapes::Circle && p == DefaultShapes::Circle::Radius)
							paramLabel = "Radius";
						else if (s.distanceFieldId == DefaultShapes::Box && p == DefaultShapes::Box::SizeX)
							paramLabel = "Size X";
						else if (s.distanceFieldId == DefaultShapes::Box && p == DefaultShapes::Box::SizeY)
							paramLabel = "Size Y";

						if (ImGui::DragFloat(paramLabel.c_str(), &s.parameters[p], 0.05f))
						{
							m_registry->setComponentDirty(s);
							m_isSceneDirty = true;
						}
					}
				}
			}

			// 5. RigidBody2D Component
			if (m_registry->hasComponent<RigidBody2D>(e))
			{
				if (ImGui::CollapsingHeader("RigidBody2D", ImGuiTreeNodeFlags_DefaultOpen))
				{
					auto& rb = m_registry->getComponent<RigidBody2D>(e);

					const char* bodyTypes[] = {"Static", "Dynamic", "Kinematic"};
					int btype = static_cast<int>(rb.type);
					if (ImGui::Combo("Body Type", &btype, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
					{
						rb.type = static_cast<BodyType>(btype);
						m_registry->setComponentDirty(rb);
						m_isSceneDirty = true;
					}

					if (ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.01f, 1000.0f))
					{
						m_registry->setComponentDirty(rb);
						m_isSceneDirty = true;
					}

					if (ImGui::Checkbox("Collision", &rb.enableCollision))
					{
						m_registry->setComponentDirty(rb);
						m_isSceneDirty = true;
					}

					if (ImGui::Checkbox("Shape Collision", &rb.enableShapeCollision))
					{
						m_registry->setComponentDirty(rb);
						m_isSceneDirty = true;
					}

					if (ImGui::Button("Remove RigidBody", ImVec2(-1.0f, 22.0f)))
					{
						m_registry->getComponentArray<RigidBody2D>()->removeData(e);
						m_isSceneDirty = true;
					}
				}
			}
			else
			{
				if (ImGui::Button("+ Add RigidBody2D", ImVec2(-1.0f, 24.0f)))
				{
					auto& rb = m_registry->addComponent<RigidBody2D>(e);
					rb.type = BodyType::Dynamic;
					rb.mass = 1.0f;
					m_isSceneDirty = true;
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("Delete Entity", ImVec2(-1.0f, 26.0f)))
			{
				deleteSelected();
			}
		}

		// =====================================================================
		// UI: Bottom Toolbar (Palette & Spawner)
		// =====================================================================
		void renderBottomToolbar()
		{
			const float winW = static_cast<float>(Display::width);
			const float winH = static_cast<float>(Display::height);

			ImGui::SetNextWindowPos(ImVec2(LEFT_PANEL_WIDTH, winH - BOTTOM_BAR_HEIGHT), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(winW - LEFT_PANEL_WIDTH - RIGHT_PANEL_WIDTH, BOTTOM_BAR_HEIGHT),
									 ImGuiCond_Always);

			const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
										   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
										   ImGuiWindowFlags_NoCollapse;

			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));

			if (ImGui::Begin("##BottomToolbar", nullptr, flags))
			{
				ImGui::TextDisabled("SPAWNER PALETTE:");
				ImGui::SameLine();

				vec2 center = getCameraCenterWorld();

				if (ImGui::Button("+ Circle"))
				{
					spawnDefaultShape(DefaultShapes::Circle, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ Box"))
				{
					spawnDefaultShape(DefaultShapes::Box, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ RoundBox"))
				{
					spawnDefaultShape(DefaultShapes::RoundBox, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ Triangle"))
				{
					spawnDefaultShape(DefaultShapes::Triangle, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ Line"))
				{
					spawnDefaultShape(DefaultShapes::Line, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ Ramp"))
				{
					spawnDefaultShape(DefaultShapes::Ramp, center);
				}
				ImGui::SameLine();
				if (ImGui::Button("+ Star"))
				{
					spawnDefaultShape(DefaultShapes::Star, center);
				}

				// Clusters dropdown if available
				const auto& clusterAssets = m_project.getClusters();
				if (!clusterAssets.empty())
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(130.0f);
					if (ImGui::BeginCombo("##SpawnClusterCombo", "+ Cluster..."))
					{
						for (const auto& ca : clusterAssets)
						{
							if (ImGui::Selectable(ca.stem.c_str()))
							{
								spawnCluster(ca.fullPath.string(), center);
							}
						}
						ImGui::EndCombo();
					}
				}

				// Custom Shapes dropdown if available
				const auto& shapeAssets = m_project.getCustomShapes();
				if (!shapeAssets.empty())
				{
					ImGui::SameLine();
					ImGui::SetNextItemWidth(130.0f);
					if (ImGui::BeginCombo("##SpawnShapeCombo", "+ Custom Shape..."))
					{
						for (const auto& sa : shapeAssets)
						{
							if (ImGui::Selectable(sa.stem.c_str()))
							{
								spawnCustomShape(sa.fullPath.string(), center);
							}
						}
						ImGui::EndCombo();
					}
				}
			}
			ImGui::End();
			ImGui::PopStyleColor();
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

			ImGui::SetNextWindowSize(ImVec2(450.0f, 280.0f), ImGuiCond_Always);
			ImGui::SetNextWindowPos(ImVec2(static_cast<float>(Display::width) * 0.5f - 225.0f,
										   static_cast<float>(Display::height) * 0.5f - 140.0f),
									ImGuiCond_Always);

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
