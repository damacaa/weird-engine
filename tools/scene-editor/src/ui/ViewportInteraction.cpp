#include "ui/ViewportInteraction.h"
#include "services/SceneOperations.h"

#include "weird-engine.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-renderer/components/Camera.h"

#include <algorithm>
#include <cmath>
#include <imgui.h>

namespace WeirdEngine::Editor
{
	void ViewportInteraction::handleHotkeys(EditorContext& ctx, SceneOperations& sceneOps)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard)
		{
			return;
		}

		if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKeyDown(Input::N))
		{
			sceneOps.newScene();
		}
		if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKeyDown(Input::O))
		{
			sceneOps.openLoadSceneDialog();
		}
		if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKeyDown(Input::S))
		{
			sceneOps.saveScenePrompt();
		}
		if (ctx.services->input().getKeyDown(Input::Space))
		{
			sceneOps.toggleSimulation();
		}
		if (ctx.services->input().getKeyDown(Input::Delete) || ctx.services->input().getKeyDown(Input::X))
		{
			sceneOps.deleteSelected();
		}
		if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKey(Input::LeftShift) &&
			ctx.services->input().getKeyDown(Input::G))
		{
			sceneOps.ungroupSelected();
		}
		else if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKeyDown(Input::G))
		{
			sceneOps.groupSelected();
		}
		if (ctx.services->input().getKey(Input::LeftCtrl) && ctx.services->input().getKeyDown(Input::D))
		{
			sceneOps.duplicateSelected();
		}
		if (ctx.services->input().getKeyDown(Input::Esc))
		{
			sceneOps.deselectAll();
		}
		if (ctx.services->input().getKeyDown(Input::F))
		{
			resetCamera(ctx);
		}
		if (ctx.services->input().getKeyDown(Input::C))
		{
			ctx.selectIndividualClusterEntities = !ctx.selectIndividualClusterEntities;
		}
		if (ctx.services->input().getKeyDown(Input::R) && ctx.selectedClusterId != 0)
		{
			auto* c = ctx.services->clusters().getById(ctx.selectedClusterId);
			if (c)
			{
				sceneOps.resetClusterToInitialPosition(*c);
			}
		}
	}

	void ViewportInteraction::handleCameraInput(EditorContext& ctx)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
		{
			return;
		}

		// Zoom with scroll wheel
		float wheel = 0.0f;
		if (ctx.services->input().getMouseButtonDown(Input::WheelUp))
		{
			wheel += 1.0f;
		}
		if (ctx.services->input().getMouseButtonDown(Input::WheelDown))
		{
			wheel -= 1.0f;
		}
		if (wheel != 0.0f)
		{
			zoomCamera(ctx, wheel);
		}

		// Middle-mouse drag pan
		bool middleDown = ctx.services->input().getMouseButton(Input::MiddleClick);
		if (middleDown && ctx.middleWasDown)
		{
			panCamera(ctx, vec2(ctx.services->input().getMouseDeltaXRaw(), -ctx.services->input().getMouseDeltaYRaw()));
		}
		ctx.middleWasDown = middleDown;
	}

	void ViewportInteraction::zoomCamera(EditorContext& ctx, float steps)
	{
		auto& cam = ctx.registry->getComponent<Transform>(ctx.services->render().getCameraEntity());
		const float oldDist = cam.position.z;
		const float newDist = std::clamp(oldDist * std::pow(0.85f, steps), 5.0f, 250.0f);
		if (std::abs(newDist - oldDist) < 0.001f)
		{
			return;
		}

		const vec2 mouse(ctx.services->input().getMouseX(), ctx.services->input().getMouseY());
		const vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, mouse);
		const float halfHeight = (std::max)(1.0f, ImGui::GetIO().DisplaySize.y * 0.5f);
		const float scale = newDist / halfHeight;

		cam.position.z = newDist;
		cam.position.x = world.x - (mouse.x - ImGui::GetIO().DisplaySize.x * 0.5f) * scale;
		cam.position.y = world.y - (mouse.y - halfHeight) * scale;
		ctx.registry->setComponentDirty(cam);
	}

	void ViewportInteraction::panCamera(EditorContext& ctx, vec2 screenDelta)
	{
		auto& cam = ctx.registry->getComponent<Transform>(ctx.services->render().getCameraEntity());
		const float halfHeight = (std::max)(1.0f, ImGui::GetIO().DisplaySize.y * 0.5f);
		const float scale = cam.position.z / halfHeight;

		cam.position.x -= screenDelta.x * scale;
		cam.position.y -= screenDelta.y * scale;
		ctx.registry->setComponentDirty(cam);
	}

	void ViewportInteraction::resetCamera(EditorContext& ctx)
	{
		auto& cam = ctx.registry->getComponent<Transform>(ctx.services->render().getCameraEntity());
		cam.position = vec3(0.0f, 0.0f, 35.0f);
		cam.rotation = vec3(0.0f, 0.0f, -1.0f);
		ctx.registry->setComponentDirty(cam);
	}

	vec2 ViewportInteraction::getMouseWorld(EditorContext& ctx)
	{
		auto& cam = ctx.registry->getComponent<Transform>(ctx.services->render().getCameraEntity());
		return ECS::Camera::screenPositionToWorldPosition2D(
			cam, vec2(ctx.services->input().getMouseX(), ctx.services->input().getMouseY()));
	}

	void ViewportInteraction::handleViewportMouseInput(EditorContext& ctx, SceneOperations& sceneOps)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureMouse)
		{
			if (ctx.isDragging)
			{
				ctx.isDragging = false;
			}
			return;
		}

		vec2 mouseWorld = getMouseWorld(ctx);

		// Left click down: selection & begin drag
		if (ctx.services->input().getMouseButtonDown(Input::LeftClick))
		{
			sceneOps.pickAtWorld(mouseWorld);
			if (ctx.hasSelection())
			{
				ctx.dragEntityStartPositions.clear();
				ctx.dragClusterStartPositions.clear();

				for (Entity e : ctx.selectedEntities)
				{
					ctx.dragEntityStartPositions[e] = sceneOps.getEntityPosition(e);
				}
				// Compute primary anchor for cluster dragging
				uint32_t primaryCId = ctx.selectedClusterId;
				vec2 primaryCentroid = mouseWorld;
				if (primaryCId != 0)
				{
					auto* primaryCluster = ctx.services->clusters().getById(primaryCId);
					if (primaryCluster)
					{
						primaryCentroid = sceneOps.computeClusterCentroid(*primaryCluster);
					}
				}

				for (uint32_t cId : ctx.selectedClusterIds)
				{
					auto* c = ctx.services->clusters().getById(cId);
					if (c)
					{
						vec2 targetPos = mouseWorld;
						if (primaryCId != 0 && cId != primaryCId)
						{
							vec2 cCentroid = sceneOps.computeClusterCentroid(*c);
							targetPos = mouseWorld + (cCentroid - primaryCentroid);
						}
						sceneOps.resetClusterShapeAtPosition(*c, targetPos);
						ctx.dragClusterStartPositions[cId] = targetPos;
					}
				}
				if (ctx.selectedGroupId != 0)
				{
					auto* g = ctx.findGroup(ctx.selectedGroupId);
					if (g)
					{
						for (Entity e : g->entities)
						{
							ctx.dragEntityStartPositions[e] = sceneOps.getEntityPosition(e);
						}
						for (uint32_t cId : g->clusterIds)
						{
							auto* c = ctx.services->clusters().getById(cId);
							if (c)
							{
								vec2 targetPos = mouseWorld;
								if (primaryCId != 0 && cId != primaryCId)
								{
									vec2 cCentroid = sceneOps.computeClusterCentroid(*c);
									targetPos = mouseWorld + (cCentroid - primaryCentroid);
								}
								sceneOps.resetClusterShapeAtPosition(*c, targetPos);
								ctx.dragClusterStartPositions[cId] = targetPos;
							}
						}
					}
				}

				ctx.isDragging = true;
				ctx.dragStartMouseWorld = mouseWorld;
				ctx.dragStartPos = sceneOps.getSelectedPosition();
			}
		}

		// Left drag: move selected object
		if (ctx.isDragging && ctx.services->input().getMouseButton(Input::LeftClick))
		{
			vec2 delta = mouseWorld - ctx.dragStartMouseWorld;
			if (ctx.gridSnap && ctx.gridSize > 0.001f)
			{
				delta.x = std::round(delta.x / ctx.gridSize) * ctx.gridSize;
				delta.y = std::round(delta.y / ctx.gridSize) * ctx.gridSize;
			}

			if (!ctx.dragEntityStartPositions.empty() || !ctx.dragClusterStartPositions.empty())
			{
				for (const auto& [e, startPos] : ctx.dragEntityStartPositions)
				{
					sceneOps.setEntityPosition(e, startPos + delta);
				}
				for (const auto& [cId, startPos] : ctx.dragClusterStartPositions)
				{
					sceneOps.setClusterPosition(cId, startPos + delta);
				}
			}
			else
			{
				vec2 targetPos = ctx.dragStartPos + delta;
				sceneOps.setSelectedPosition(targetPos);
			}
			ctx.isSceneDirty = true;
		}

		if (ctx.isDragging && !ctx.services->input().getMouseButton(Input::LeftClick))
		{
			ctx.isDragging = false;
		}

		// Right-click in viewport: open creation context menu at clicked world position
		if (ctx.services->input().getMouseButtonDown(Input::RightClick))
		{
			ctx.contextMenuWorldPos = mouseWorld;
			ctx.openViewportContextMenu = true;
		}
	}

	void ViewportInteraction::renderBottomToolbar(EditorContext& ctx, SceneOperations& sceneOps)
	{
		const float winW = ImGui::GetIO().DisplaySize.x;
		const float winH = ImGui::GetIO().DisplaySize.y;
		const float bottomW = winW - EditorContext::LEFT_PANEL_WIDTH - EditorContext::RIGHT_PANEL_WIDTH;

		// Positioned flush between left panel and right panel at the very bottom
		ImGui::SetNextWindowPos(ImVec2(EditorContext::LEFT_PANEL_WIDTH, winH - EditorContext::BOTTOM_BAR_HEIGHT),
								ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(bottomW, EditorContext::BOTTOM_BAR_HEIGHT), ImGuiCond_Always);

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
			if (ctx.selectIndividualClusterEntities)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.1f, 1.0f));
				if (ImGui::Button("Move: Rigidbody (C)", ImVec2(190.0f, 26.0f)))
				{
					ctx.selectIndividualClusterEntities = false;
				}
				ImGui::PopStyleColor();
			}
			else
			{
				if (ImGui::Button("Move: Entire Cluster (C)", ImVec2(190.0f, 26.0f)))
				{
					ctx.selectIndividualClusterEntities = true;
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Toggle whether clicking a cluster moves the whole cluster or selects/moves an "
								  "individual rigidbody.\nShortcut: Press 'C' or hold 'Alt' while clicking.");
			}

			// Simulation Play/Pause button
			ImGui::SameLine(bottomW - 140.0f);
			if (ctx.isSimulating)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.2f, 1.0f));
				if (ImGui::Button("Pause (Space)", ImVec2(120.0f, 26.0f)))
				{
					sceneOps.toggleSimulation();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.3f, 1.0f));
				if (ImGui::Button("Play (Space)", ImVec2(120.0f, 26.0f)))
				{
					sceneOps.toggleSimulation();
				}
				ImGui::PopStyleColor();
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(3);
		ImGui::PopStyleColor();
	}

	void ViewportInteraction::renderViewportContextMenu(EditorContext& ctx, SceneOperations& sceneOps)
	{
		if (ctx.openViewportContextMenu)
		{
			ImGui::OpenPopup("ViewportCreationContextMenu");
			ctx.openViewportContextMenu = false;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));
		if (ImGui::BeginPopup("ViewportCreationContextMenu"))
		{
			vec2 spawnPos = ctx.contextMenuWorldPos;
			if (ctx.gridSnap && ctx.gridSize > 0.001f)
			{
				spawnPos.x = std::round(spawnPos.x / ctx.gridSize) * ctx.gridSize;
				spawnPos.y = std::round(spawnPos.y / ctx.gridSize) * ctx.gridSize;
			}

			ImGui::TextDisabled("Create at (%.2f, %.2f)", spawnPos.x, spawnPos.y);
			ImGui::Separator();

			// 1. Primitives
			if (ImGui::BeginMenu("Primitives"))
			{
				if (ImGui::MenuItem("Circle"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Circle, spawnPos);
				}
				if (ImGui::MenuItem("Box"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Box, spawnPos);
				}
				if (ImGui::MenuItem("RoundBox"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::RoundBox, spawnPos);
				}
				if (ImGui::MenuItem("Triangle"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Triangle, spawnPos);
				}
				if (ImGui::MenuItem("Line"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Line, spawnPos);
				}
				if (ImGui::MenuItem("Ramp"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Ramp, spawnPos);
				}
				if (ImGui::MenuItem("Star"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::Star, spawnPos);
				}
				ImGui::EndMenu();
			}

			// 2. Clusters (From Assets)
			const auto& clusterAssets = ctx.project.getClusters();
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
						const char* label = !ca.relativePath.empty() ? ca.relativePath.c_str() : ca.fileName.c_str();
						if (ImGui::MenuItem(label))
						{
							sceneOps.spawnCluster(ca.fullPath.string(), spawnPos);
						}
					}
				}
				ImGui::EndMenu();
			}

			// 3. Custom Shapes (From Assets)
			const auto& shapeAssets = ctx.project.getCustomShapes();
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
						const char* label = !sa.relativePath.empty() ? sa.relativePath.c_str() : sa.fileName.c_str();
						if (ImGui::MenuItem(label))
						{
							sceneOps.spawnCustomShape(sa.fullPath.string(), spawnPos);
						}
					}
				}
				ImGui::EndMenu();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);
	}

	void ViewportInteraction::renderProjectModal(EditorContext& ctx)
	{
		if (ctx.project.isOpen())
		{
			return;
		}

		const float winW = ImGui::GetIO().DisplaySize.x;
		const float winH = ImGui::GetIO().DisplaySize.y;

		ImGui::SetNextWindowSize(ImVec2(450.0f, 280.0f), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(winW * 0.5f - 225.0f, winH * 0.5f - 140.0f), ImGuiCond_Always);

		if (ImGui::Begin("Welcome to Scene Editor", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::TextWrapped("Select a project folder containing an 'assets/' directory to begin editing.");
			ImGui::Spacing();

			if (ImGui::Button("Open Project Folder...", ImVec2(-1.0f, 32.0f)))
			{
				ctx.project.openFolderDialog();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Text("Recent Projects:");

			if (ctx.project.getRecentProjects().empty())
			{
				ImGui::TextDisabled("No recent projects found.");
			}
			else
			{
				for (const auto& p : ctx.project.getRecentProjects())
				{
					if (ImGui::Selectable(p.c_str()))
					{
						ctx.project.open(p);
					}
				}
			}

			ImGui::End();
		}
	}
} // namespace WeirdEngine::Editor
