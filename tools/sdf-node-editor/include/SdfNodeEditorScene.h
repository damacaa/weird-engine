#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <imgui_internal.h>
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

			// Define material 0 with default light blue accent color (UI Shape / Audio preview)
			auto& mat0 = services.materials2D().get(0);
			mat0.name = "sdf_preview";
			mat0.color = glm::vec4(0.15f, 0.85f, 0.95f, 1.0f);

			// Define material 1 with distinct warm amber accent color (World CustomShape preview)
			auto& mat1 = services.materials2D().getOrCreate("world_preview");
			mat1.color = glm::vec4(1.0f, 0.58f, 0.16f, 1.0f);

			// Define material 2 for spawned physics dots (bright warm yellow/white)
			auto& matDot = services.materials2D().getOrCreate("preview_dot");
			matDot.color = glm::vec4(1.0f, 0.92f, 0.35f, 1.0f);
			m_dotMaterialId = matDot.id;

			// Load default starter graph
			m_graph.loadPresetStar();
			m_parameters = m_graph.getParameters();
			m_dirty = true;

			loadRecentFiles();
		}

		void onStart(Registry& registry, ServiceProvider& services) override
		{
			Entity mainCam = services.tags().getEntityByTag("mainCamera");
			if (mainCam < MAX_ENTITIES && registry.hasComponent<Transform>(mainCam))
			{
				auto& t = registry.getComponent<Transform>(mainCam);
				t.position.z = m_cameraZoom;
				if (registry.hasComponent<FlyMovement2D>(mainCam))
				{
					auto& fly = registry.getComponent<FlyMovement2D>(mainCam);
					fly.targetPosition = t.position;
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
			clearSpawnedDots(registry);
			if (m_worldEntity != INVALID_ENTITY && registry.hasComponent<CustomShape>(m_worldEntity))
			{
				registry.destroyEntity(m_worldEntity);
				m_worldEntity = INVALID_ENTITY;
			}
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

			// In World mode: handle physics dots spawning on click and boundary destruction
			if (m_previewMode == PreviewMode::WorldShape)
			{
				int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
				int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;
				float menuH = 24.0f;
				float contentH = static_cast<float>(winH) - menuH;
				float inspectorWidth = 360.0f;
				float canvasWidth = static_cast<float>(winW) - inspectorWidth;
				float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
				float inspectorHeight = contentH - previewHeight;

				float gapMinX = canvasWidth;
				float gapMaxX = static_cast<float>(winW);
				float gapMinY = menuH + inspectorHeight;
				float gapMaxY = static_cast<float>(winH);
				float selectorMinX = gapMaxX - kPreviewSelectorWidth - kPreviewSelectorRightMargin;

				Entity mainCam = services.tags().getEntityByTag("mainCamera");
				if (mainCam != INVALID_ENTITY && registry.hasComponent<Transform>(mainCam))
				{
					auto& camTransform = registry.getComponent<Transform>(mainCam);

					// 1. Mouse wheel zoom: scroll in/out over the preview window to adjust camera zoom
					float mouseScreenX = services.input().getMouseX();
					float mouseCameraY = services.input().getMouseY(); // Display::height - rawSDL_Y
					float mouseTopLeftY = static_cast<float>(winH) - mouseCameraY;

					bool isInPreviewGap = (mouseScreenX >= gapMinX && mouseScreenX <= gapMaxX &&
										   mouseTopLeftY >= gapMinY && mouseTopLeftY <= gapMaxY);
					// The selector occupies the preview's top-right strip. Keep it out of
					// the world interaction area, including the combo popup below it.
					bool isInPreviewSelector = (mouseScreenX >= selectorMinX && mouseScreenX <= gapMaxX &&
												mouseTopLeftY >= gapMinY && mouseTopLeftY <= gapMaxY);
					bool isHoveredInGap = isInPreviewGap && !isInPreviewSelector;

					if (isHoveredInGap)
					{
						float wheelDelta = 0.0f;
						if (services.input().getMouseButtonDown(Input::WheelUp))
							wheelDelta += 1.0f;
						if (services.input().getMouseButtonDown(Input::WheelDown))
							wheelDelta -= 1.0f;

						const ImGuiIO& io = ImGui::GetIO();
						if (wheelDelta == 0.0f && std::abs(io.MouseWheel) > 0.01f)
						{
							wheelDelta = io.MouseWheel;
						}

						if (wheelDelta != 0.0f)
						{
							// Scrolling up zooms in (lowers zoom distance / expands view),
							// scrolling down zooms out (increases zoom distance).
							// Scale proportionally to current zoom for smooth zooming across ranges.
							float zoomFactor = (wheelDelta > 0.0f) ? 0.90f : 1.10f;
							m_cameraZoom = std::clamp(m_cameraZoom * zoomFactor, 5.0f, 200.0f);
							glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f,
												   previewHeight * 0.5f);
							updateCameraPosition(services, winW, winH, targetCenter);
						}
					}

					// 2. Mouse spawning: check if left mouse is down or held inside the preview window.
					// With HitTestHole on the preview gap, ImGui does not capture the mouse in this hole,
					// so services.input().getMouseButton(Input::LeftClick) works natively as expected.
					bool isLeftDown =
						services.input().getMouseButton(Input::LeftClick) || ImGui::IsMouseDown(ImGuiMouseButton_Left);

					if (isLeftDown)
					{
						if (isHoveredInGap)
						{
							static float lastSpawnTime = 0.0f;
							if (services.time().time() - lastSpawnTime > 0.5f) // spawn every 50ms while held
							{
								lastSpawnTime = services.time().time();

								// Convert screen coordinates to world coordinates.
								glm::vec2 mousePosForCam(mouseScreenX, mouseCameraY);
								glm::vec2 worldPos =
									ECS::Camera::screenPositionToWorldPosition2D(camTransform, mousePosForCam);

								// Spawn a burst of dots (e.g. 10 dots each frame button is down)
								int countToSpawn = 1;
								for (int i = 0; i < countToSpawn; ++i)
								{
									if (m_spawnedDots.size() >= 3000)
										break;

									Entity dotEntity = registry.createEntity();
									auto& t = registry.addComponent<Transform>(dotEntity);

									float offsetX = ((std::rand() % 200) - 100.0f) * 0.01f * 0.5f;
									float offsetY = ((std::rand() % 200) - 100.0f) * 0.01f * 0.5f;
									t.position = glm::vec3(worldPos.x + offsetX, worldPos.y + offsetY, 0.0f);

									auto& dot = registry.addComponent<Dot>(dotEntity);
									dot.materialId = m_dotMaterialId;

									auto& rb = registry.addComponent<RigidBody2D>(dotEntity);

									m_spawnedDots.push_back(dotEntity);
								}
							}
						}
					}

					// 2. Destroy dots that leave the preview window
					float margin = 20.0f;
					for (size_t i = 0; i < m_spawnedDots.size();)
					{
						Entity e = m_spawnedDots[i];
						if (!registry.hasComponent<Transform>(e))
						{
							m_spawnedDots[i] = m_spawnedDots.back();
							m_spawnedDots.pop_back();
							continue;
						}

						auto& t = registry.getComponent<Transform>(e);
						glm::vec2 screenPos = ECS::Camera::worldPosition2DToScreenPosition(
							camTransform, glm::vec2(t.position.x, t.position.y));
						float screenTopLeftY = static_cast<float>(winH) - screenPos.y;

						bool outOfBounds = (screenPos.x < (gapMinX - margin)) || (screenPos.x > (gapMaxX + margin)) ||
										   (screenTopLeftY < (gapMinY - margin)) ||
										   (screenTopLeftY > (gapMaxY + margin));

						if (outOfBounds)
						{
							registry.destroyEntity(e);
							m_spawnedDots[i] = m_spawnedDots.back();
							m_spawnedDots.pop_back();
						}
						else
						{
							++i;
						}
					}
				}
			}
		}

		void onCustomUI(Registry& registry, ServiceProvider& services) override
		{
			renderEditorUI(services);
		}

	private:
		void updateCameraPosition(ServiceProvider& services, int winW, int winH, glm::vec2 targetCenter)
		{
			float uvCenterX = ((2.0f * targetCenter.x / static_cast<float>(winW)) - 1.0f) *
							  (static_cast<float>(winW) / static_cast<float>(winH));
			float uvCenterY = (2.0f * targetCenter.y / static_cast<float>(winH)) - 1.0f;

			Entity mainCam = services.tags().getEntityByTag("mainCamera");
			if (mainCam < MAX_ENTITIES && services.registry().hasComponent<Transform>(mainCam))
			{
				auto& t = services.registry().getComponent<Transform>(mainCam);
				float zoom = m_cameraZoom > 0.0f ? m_cameraZoom : 35.0f;
				glm::vec3 desiredPos(-zoom * uvCenterX, -zoom * uvCenterY, zoom);

				if (m_previewMode == PreviewMode::UIShape)
				{
					desiredPos.x += 100000.0f;
					desiredPos.y += 100000.0f;
				}

				if (glm::distance(t.position, desiredPos) > 0.001f)
				{
					t.position = desiredPos;
					services.registry().setComponentDirty(t);
				}
				if (services.registry().hasComponent<FlyMovement2D>(mainCam))
				{
					auto& fly = services.registry().getComponent<FlyMovement2D>(mainCam);
					fly.targetPosition = desiredPos;
				}
			}
		}

		void syncParameters(ServiceProvider& services)
		{
			m_graph.setParameters(m_parameters);

			// 1. Update World CustomShape parameters
			if (m_worldEntity != INVALID_ENTITY && services.registry().hasComponent<CustomShape>(m_worldEntity))
			{
				auto& cs = services.registry().getComponent<CustomShape>(m_worldEntity);
				std::copy_n(m_parameters.data(), 8, cs.parameters);
				services.registry().setComponentDirty(cs);
			}

			// 2. Update UI Shape parameters
			if (m_previewEntity != INVALID_ENTITY && services.registry().hasComponent<UIShape>(m_previewEntity))
			{
				auto& uiShape = services.registry().getComponent<UIShape>(m_previewEntity);
				std::copy_n(m_parameters.data(), 8, uiShape.parameters);
				services.registry().setComponentDirty(uiShape);
			}

			// 3. Update Audio Song parameters
			if (m_song)
			{
				for (size_t i = 0; i < 8; ++i)
				{
					m_song->setParameter(i, m_parameters[i]);
					services.audio().setSongParameter(i, m_parameters[i]);
				}
				m_song->calculateMusicalPropertiesFromShape();
			}

			services.audio().resampleShape();
		}

		void rebuildAndSync(ServiceProvider& services)
		{
			int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
			int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;
			float menuH = 24.0f;
			float contentH = static_cast<float>(winH) - menuH;
			float inspectorWidth = 360.0f;
			float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
			glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);
			m_lastPreviewTargetCenter = targetCenter;

			updateCameraPosition(services, winW, winH, targetCenter);

			m_evaluatedExpr = m_graph.evaluate();
			if (!m_evaluatedExpr.node)
			{
				m_evaluatedExpr = Expr(0.0f);
			}

			// 1. Procedural Song (UI Shape)
			// Keep only the selected preview mode visible in the preview area.
			glm::vec2 uiCenter =
				(m_previewMode == PreviewMode::UIShape) ? targetCenter : glm::vec2(100000.0f, 100000.0f);
			if (m_song)
			{
				m_song->setCenter(uiCenter);
				m_song->setShapeExpression(m_evaluatedExpr.node);
				for (size_t i = 0; i < 8; ++i)
				{
					m_song->setParameter(i, m_parameters[i]);
				}
				m_song->calculateMusicalPropertiesFromShape();
			}
			else
			{
				m_song = WeirdRenderer::SdfSong::create("node_editor_song", m_evaluatedExpr, uiCenter);
				for (size_t i = 0; i < 8; ++i)
				{
					m_song->setParameter(i, m_parameters[i]);
				}
			}

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
				std::copy_n(m_parameters.data(), 8, uiShape.parameters);
				services.registry().setComponentDirty(uiShape);
			}

			// 2. World Shape (CustomShape component on separate entity with Material 1)
			std::shared_ptr<IMathExpression> worldExpr = m_evaluatedExpr.node;

			if (m_worldEntity != INVALID_ENTITY && services.registry().hasComponent<CustomShape>(m_worldEntity))
			{
				services.registry().destroyEntity(m_worldEntity);
				m_worldEntity = INVALID_ENTITY;
			}

			ShapeId worldShapeId = services.shapes().registerSDF(worldExpr ? worldExpr : Expr(0.0f).node);
			auto& mat1 = services.materials2D().getOrCreate("world_preview");
			ShapeConfig worldConfig;
			worldConfig.shapeId = worldShapeId;
			worldConfig.material = mat1;
			worldConfig.combination = CombinationType::Addition;
			worldConfig.hasCollision = true;
			worldConfig.group = 0;
			std::copy_n(m_parameters.data(), 8, worldConfig.variables.data);

			m_worldEntity = services.shapes().addShape(worldConfig);
			if (m_worldEntity != INVALID_ENTITY && services.registry().hasComponent<CustomShape>(m_worldEntity))
			{
				auto& cs = services.registry().getComponent<CustomShape>(m_worldEntity);
				cs.smoothFactor = 0.0f;
				cs.material = mat1.id;
				std::copy_n(m_parameters.data(), 8, cs.parameters);
				services.registry().setComponentDirty(cs);
			}

			services.render().forceShaderRefresh();
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

				// Keep preview centered inside preview gap if window dimensions change
				glm::vec2 targetCenter(static_cast<float>(winW) - inspectorWidth * 0.5f, previewHeight * 0.5f);
				if (glm::distance(m_lastPreviewTargetCenter, targetCenter) > 1.0f)
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

				// Keep the preview selector in the preview area so it is independent of the inspector tabs above it.
				ImGui::SetNextWindowPos(ImVec2(gapMax.x - kPreviewSelectorWidth - kPreviewSelectorRightMargin,
											   gapMin.y + kPreviewSelectorTopMargin),
										ImGuiCond_Always);
				ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.14f, 0.14f, 0.14f, 0.92f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.26f, 0.26f, 0.26f, 0.85f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

				ImGuiWindowFlags selectorFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
												 ImGuiWindowFlags_NoSavedSettings |
												 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
												 ImGuiWindowFlags_NoMove;

				if (ImGui::Begin("##PreviewModeSelector", nullptr, selectorFlags))
				{
					const bool worldSelected = m_previewMode == PreviewMode::WorldShape;
					if (worldSelected)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.65f, 1.0f));
					if (ImGui::Button("World", ImVec2(50.0f, 0.0f)))
					{
						if (!worldSelected)
						{
							m_previewMode = PreviewMode::WorldShape;
							m_dirty = true;
						}
					}
					if (worldSelected)
						ImGui::PopStyleColor();

					ImGui::SameLine(0.0f, 4.0f);
					const bool uiSelected = m_previewMode == PreviewMode::UIShape;
					if (uiSelected)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.65f, 1.0f));
					if (ImGui::Button("UI", ImVec2(50.0f, 0.0f)))
					{
						if (!uiSelected)
						{
							m_previewMode = PreviewMode::UIShape;
							m_dirty = true;
						}
					}
					if (uiSelected)
						ImGui::PopStyleColor();
					ImGui::End();
				}

				ImGui::PopStyleVar(3);
				ImGui::PopStyleColor(2);

				// Define a hit-test hole on the root window covering the preview gap.
				// This causes ImGui to treat this rectangular region as a transparent hole,
				// so ImGui window hovering fails here and io.WantCaptureMouse becomes false.
				ImGuiWindow* rootWin = ImGui::GetCurrentWindow();
				if (rootWin)
				{
					rootWin->HitTestHoleOffset = ImVec2ih(static_cast<short>(gapMin.x - rootWin->Pos.x),
														  static_cast<short>(gapMin.y - rootWin->Pos.y));
					rootWin->HitTestHoleSize =
						ImVec2ih(static_cast<short>(gapMax.x - gapMin.x), static_cast<short>(gapMax.y - gapMin.y));
				}
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
						m_parameters = m_graph.getParameters();
						m_graph.addNode("sdf_output", {400.0f, 200.0f});
						m_dirty = true;
					}
					if (ImGui::MenuItem("Open Graph JSON...", "Ctrl+O"))
					{
						openLoadFileDialog();
					}
					if (ImGui::BeginMenu("Recent", !m_recentFiles.empty()))
					{
						std::string toLoad;
						for (size_t i = 0; i < m_recentFiles.size(); ++i)
						{
							const auto& filePath = m_recentFiles[i];
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
							m_recentFiles.clear();
							saveRecentFiles();
						}
						ImGui::EndMenu();

						if (!toLoad.empty())
						{
							loadGraphFromFile(toLoad);
						}
					}
					if (ImGui::MenuItem("Save Graph JSON...", "Ctrl+S"))
					{
						openSaveFileDialog();
					}
					ImGui::Separator();
					if (ImGui::BeginMenu("Presets"))
					{
						if (ImGui::MenuItem("Star (Procedural Song)"))
						{
							m_graph.loadPresetStar();
							m_parameters = m_graph.getParameters();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("Aquatic Wave (Procedural Song)"))
						{
							m_graph.loadPresetAquaticWave();
							m_parameters = m_graph.getParameters();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("CSG Ring (Subtract)"))
						{
							m_graph.loadPresetCsgRing();
							m_parameters = m_graph.getParameters();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("Basic Circle"))
						{
							m_graph.loadPresetCircle();
							m_parameters = m_graph.getParameters();
							m_syncPositionsToImNodes = true;
							ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
							m_dirty = true;
						}
						if (ImGui::MenuItem("Infinite Repeat (Modulo Grid)"))
						{
							m_graph.loadPresetInfiniteRepeat();
							m_parameters = m_graph.getParameters();
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

			float zoom = ImNodes::EditorContextGetZoom();

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
					ImGui::PushItemWidth(85.0f * zoom);
					if (ImGui::DragFloat("##val", &node.data.customFloat, 0.1f, -1000.0f, 1000.0f, "%.2f"))
						m_dirty = true;
					ImGui::PopItemWidth();
				}
				else if (node.typeId == "param_var")
				{
					ImGui::PushItemWidth(70.0f * zoom);
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
						ImGui::PushItemWidth(75.0f * zoom);
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
			if (ImGui::BeginTabBar("InspectorTabBar", ImGuiTabBarFlags_None))
			{
				InspectorTab selectedTab = m_activeTab;

				// =============================================================
				// Tab 1: Shape Parameters & World Preview (Default)
				// =============================================================
				if (ImGui::BeginTabItem("Parameters"))
				{
					selectedTab = InspectorTab::Parameters;
					renderParametersTab(services);
					ImGui::EndTabItem();
				}

				// =============================================================
				// Tab 2: Procedural Music & Audio
				// =============================================================
				if (ImGui::BeginTabItem("Audio & Song"))
				{
					selectedTab = InspectorTab::Audio;
					renderAudioTab(services);
					ImGui::EndTabItem();
				}

				// =============================================================
				// Tab 3: Shader & Code Exporter
				// =============================================================
				if (ImGui::BeginTabItem("Code & Shader"))
				{
					selectedTab = InspectorTab::CodeShader;
					renderCodeShaderTab(services);
					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();

				if (selectedTab != m_activeTab)
				{
					if (m_activeTab == InspectorTab::Parameters && selectedTab != InspectorTab::Parameters)
					{
						clearSpawnedDots(services.registry());
					}
					m_activeTab = selectedTab;
				}
			}
		}

		void renderParametersTab(ServiceProvider& services)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "World Shape Parameters (var0 .. var7)");
			ImGui::TextDisabled("Dynamic parameters bound to CustomShape (var0..var7).");
			ImGui::Separator();
			ImGui::Spacing();

			for (int i = 0; i < 8; ++i)
			{
				char idStr[32];
				snprintf(idStr, sizeof(idStr), "##var%d", i);

				ImGui::PushID(i);
				ImGui::TextColored(ImVec4(0.85f, 0.85f, 0.85f, 1.0f), "var%d", i);
				ImGui::SameLine(50.0f);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragFloat(idStr, &m_parameters[i], 0.1f, -1000.0f, 1000.0f, "%.2f"))
				{
					syncParameters(services);
				}
				ImGui::PopID();
			}

			ImGui::Spacing();
			ImGui::TextWrapped("Tip: In your node graph, add 'Variable (var0..var7)' from the Input category to link "
							   "these variables to shape sizes, positions, or math nodes.");

			ImGui::Spacing();
			if (ImGui::CollapsingHeader("Physics Simulation", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text("Active Dots: %zu", m_spawnedDots.size());
				if (ImGui::Button("Clear Spawned Dots", ImVec2(-1.0f, 22.0f)))
				{
					clearSpawnedDots(services.registry());
				}
				ImGui::TextDisabled("Click/drag inside the preview window to spawn physics dots!");
			}
		}

		void renderAudioTab(ServiceProvider& services)
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
		}

		void renderCodeShaderTab(ServiceProvider& services)
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Live GLSL AST Shader Output");
			ImGui::Separator();

			if (ImGui::Button("Copy GLSL Code", ImVec2(-1.0f, 25.0f)))
			{
				ImGui::SetClipboardText(m_cachedGlslCode.c_str());
			}

			ImGui::InputTextMultiline("##GlslCodeBox", const_cast<char*>(m_cachedGlslCode.data()),
									  m_cachedGlslCode.size(), ImVec2(-1.0f, 150.0f), ImGuiInputTextFlags_ReadOnly);

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "C++ Expr Code Snippet");
			ImGui::Separator();

			if (ImGui::Button("Copy C++ Code", ImVec2(-1.0f, 25.0f)))
			{
				ImGui::SetClipboardText(m_cachedCppCode.c_str());
			}

			ImGui::InputTextMultiline("##CppCodeBox", const_cast<char*>(m_cachedCppCode.data()), m_cachedCppCode.size(),
									  ImVec2(-1.0f, 200.0f), ImGuiInputTextFlags_ReadOnly);
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
				addRecentFile(path);
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
					m_parameters = m_graph.getParameters();
					m_syncPositionsToImNodes = true;
					m_dirty = true;
					addRecentFile(path);
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

		void addRecentFile(const std::string& path)
		{
			if (path.empty())
				return;

			// Normalize separators to forward slashes for clean presentation
			std::string normPath = path;
			std::replace(normPath.begin(), normPath.end(), '\\', '/');

			auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), normPath);
			if (it != m_recentFiles.end())
			{
				m_recentFiles.erase(it);
			}
			m_recentFiles.push_front(normPath);
			while (m_recentFiles.size() > 10)
			{
				m_recentFiles.pop_back();
			}
			saveRecentFiles();
		}

		void loadRecentFiles()
		{
			m_recentFiles.clear();
			std::ifstream f("sdf_editor_recent.json");
			if (f.is_open())
			{
				try
				{
					json j;
					f >> j;
					f.close();
					if (j.is_array())
					{
						for (const auto& item : j)
						{
							if (item.is_string())
							{
								std::string p = item.get<std::string>();
								if (!p.empty())
									m_recentFiles.push_back(p);
							}
						}
					}
				}
				catch (...)
				{
				}
			}
		}

		void saveRecentFiles()
		{
			std::ofstream f("sdf_editor_recent.json");
			if (f.is_open())
			{
				json j = json::array();
				for (const auto& p : m_recentFiles)
				{
					j.push_back(p);
				}
				f << j.dump(2);
			}
		}

		std::mutex m_fileActionMutex;
		std::string m_pendingLoadPath;
		std::string m_pendingSavePath;
		std::deque<std::string> m_recentFiles;

		NodeGraph m_graph;
		Expr m_evaluatedExpr;
		std::shared_ptr<WeirdRenderer::SdfSong> m_song;
		Entity m_previewEntity = INVALID_ENTITY;
		Entity m_worldEntity = INVALID_ENTITY;

		std::array<float, 8> m_parameters = {0.0f};
		float m_cameraZoom = 35.0f;
		glm::vec2 m_lastPreviewTargetCenter = {0.0f, 0.0f};

		enum class InspectorTab
		{
			Parameters,
			Audio,
			CodeShader
		};
		InspectorTab m_activeTab = InspectorTab::Parameters;

		enum class PreviewMode
		{
			WorldShape,
			UIShape
		};
		PreviewMode m_previewMode = PreviewMode::WorldShape;
		static constexpr float kPreviewSelectorWidth = 118.0f;
		static constexpr float kPreviewSelectorRightMargin = 8.0f;
		static constexpr float kPreviewSelectorTopMargin = 5.0f;

		bool m_dirty = true;
		bool m_syncPositionsToImNodes = true;
		bool m_playSong = true;
		float m_musicVolume = 0.75f;

		unsigned int m_dotMaterialId = 0;
		std::vector<Entity> m_spawnedDots;

		void clearSpawnedDots(Registry& registry)
		{
			for (Entity e : m_spawnedDots)
			{
				if (e != INVALID_ENTITY && registry.hasComponent<Transform>(e))
				{
					registry.destroyEntity(e);
				}
			}
			m_spawnedDots.clear();
		}

		std::string m_cachedGlslCode;
		std::string m_cachedCppCode;
	};
} // namespace WeirdEngine::Editor
