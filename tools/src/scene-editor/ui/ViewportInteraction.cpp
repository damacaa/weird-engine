#include "scene-editor/ui/ViewportInteraction.h"
#include "scene-editor/services/SceneOperations.h"
#include "ToolsSession.h"
#include "UndoHistory.h"

#include "weird-engine.h"
#include "weird-engine/components/Transform.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/Shape.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <imgui.h>
#include <limits>
#include <unordered_set>

namespace WeirdEngine::Tools::SceneEd
{
	namespace
	{
		constexpr float kGizmoAxisLengthPx = 40.0f;
		constexpr float kGizmoHandleHitRadiusPx = 9.5f;
		constexpr float kGizmoCenterHitRadiusPx = 8.5f;
		constexpr float kGizmoHandleDrawRadiusPx = 5.5f;
		constexpr float kGizmoHandleHoverRadiusPx = 7.5f;
		constexpr float kTwoPi = 6.28318530718f;

		struct GizmoLayout
		{
			bool visible = false;
			bool isLine = false;
			bool isVerticalOnly = false;
			Entity targetEntity = INVALID_ENTITY;
			vec2 centerWorld{0.0f, 0.0f};
			vec2 lineStartWorld{0.0f, 0.0f};
			vec2 lineEndWorld{0.0f, 0.0f};
		};

		ImVec2 worldToImGuiScreen(const Transform& cam, float winH, vec2 worldPos)
		{
			const vec2 screen = ECS::Camera::worldPosition2DToScreenPosition(cam, worldPos);
			return ImVec2(screen.x, winH - screen.y);
		}

		float distPx(ImVec2 a, ImVec2 b)
		{
			return std::hypot(a.x - b.x, a.y - b.y);
		}

		void gatherEffectiveSelection(const EditorContext& ctx, std::unordered_set<Entity>& outEntities,
									  std::unordered_set<uint32_t>& outClusters)
		{
			outClusters = ctx.selectedClusterIds;
			if (ctx.selectedClusterId != 0)
			{
				outClusters.insert(ctx.selectedClusterId);
			}
			outEntities = ctx.selectedEntities;
			if (ctx.selectedEntity != INVALID_ENTITY)
			{
				outEntities.insert(ctx.selectedEntity);
			}
			if (ctx.selectedGroupId != 0)
			{
				if (const auto* g = ctx.findGroup(ctx.selectedGroupId))
				{
					for (Entity ge : g->entities)
					{
						outEntities.insert(ge);
					}
					for (uint32_t gc : g->clusterIds)
					{
						outClusters.insert(gc);
					}
				}
			}

			const Entity camEntity = ctx.services ? ctx.services->render().getCameraEntity() : INVALID_ENTITY;
			for (uint32_t cId : outClusters)
			{
				if (const auto* c = ctx.services->clusters().getById(cId))
				{
					for (Entity member : c->entities)
					{
						outEntities.erase(member);
					}
				}
			}
			for (auto it = outEntities.begin(); it != outEntities.end();)
			{
				Entity e = *it;
				if (e == INVALID_ENTITY || e == camEntity || ctx.registry == nullptr ||
					ctx.registry->isEntityDestroyed(e))
				{
					it = outEntities.erase(it);
				}
				else
				{
					++it;
				}
			}
			for (auto it = outClusters.begin(); it != outClusters.end();)
			{
				if (ctx.services == nullptr || ctx.services->clusters().getById(*it) == nullptr)
				{
					it = outClusters.erase(it);
				}
				else
				{
					++it;
				}
			}
		}

		GizmoLayout computeActiveGizmoLayout(const EditorContext& ctx, const SceneOperations& sceneOps,
											 const Transform& cam)
		{
			GizmoLayout layout;
			if (ctx.registry == nullptr || ctx.services == nullptr)
			{
				return layout;
			}

			std::unordered_set<Entity> entities;
			std::unordered_set<uint32_t> clusters;
			gatherEffectiveSelection(ctx, entities, clusters);
			if (entities.empty() && clusters.empty())
			{
				return layout;
			}

			layout.visible = true;
			if (entities.size() == 1 && clusters.empty())
			{
				Entity e = *entities.begin();
				layout.targetEntity = e;
				if (ctx.registry->hasComponent<Shape>(e))
				{
					const auto& s = ctx.registry->getComponent<Shape>(e);
					if (s.distanceFieldId == DefaultShapes::Line)
					{
						layout.isLine = true;
						layout.lineStartWorld = sceneOps.getLineEndpoint(e, 0);
						layout.lineEndWorld = sceneOps.getLineEndpoint(e, 1);
						layout.centerWorld = (layout.lineStartWorld + layout.lineEndWorld) * 0.5f;
						return layout;
					}
					if (s.distanceFieldId == DefaultShapes::SineWave)
					{
						const bool hasTrans = ctx.registry->hasComponent<Transform>(e);
						layout.isVerticalOnly = !hasTrans;
						layout.centerWorld = hasTrans
												 ? sceneOps.getEntityPosition(e)
												 : vec2(cam.position.x, s.parameters[DefaultShapes::SineWave::Offset]);
						return layout;
					}
				}
				layout.centerWorld = sceneOps.getEntityPosition(e);
				return layout;
			}

			if (clusters.size() == 1 && entities.empty())
			{
				if (const auto* c = ctx.services->clusters().getById(*clusters.begin()))
				{
					layout.centerWorld = c->position;
					return layout;
				}
			}

			vec2 accum(0.0f);
			size_t count = 0;
			for (Entity e : entities)
			{
				accum += sceneOps.getEntityPosition(e);
				++count;
			}
			for (uint32_t cId : clusters)
			{
				if (const auto* c = ctx.services->clusters().getById(cId))
				{
					accum += c->position;
					++count;
				}
			}
			if (count > 0)
			{
				layout.centerWorld = accum / static_cast<float>(count);
			}
			return layout;
		}

		ViewportGizmoHandle hitTestGizmoHandle(const GizmoLayout& layout, const Transform& cam, float winH,
											   vec2 worldPos)
		{
			if (!layout.visible || cam.position.z == 0.0f)
			{
				return ViewportGizmoHandle::None;
			}

			const ImVec2 mousePx = worldToImGuiScreen(cam, winH, worldPos);
			if (layout.isLine)
			{
				const ImVec2 startPx = worldToImGuiScreen(cam, winH, layout.lineStartWorld);
				const ImVec2 endPx = worldToImGuiScreen(cam, winH, layout.lineEndWorld);
				const float lineLenPx = distPx(startPx, endPx);
				const float hitRadius =
					(lineLenPx < 22.0f) ? (std::max)(5.0f, lineLenPx * 0.30f) : kGizmoHandleHitRadiusPx;

				if (distPx(mousePx, endPx) <= hitRadius)
				{
					return ViewportGizmoHandle::LineEnd;
				}
				if (distPx(mousePx, startPx) <= hitRadius)
				{
					return ViewportGizmoHandle::LineStart;
				}
				return ViewportGizmoHandle::None;
			}

			const ImVec2 centerPx = worldToImGuiScreen(cam, winH, layout.centerWorld);
			if (distPx(mousePx, centerPx) <= kGizmoCenterHitRadiusPx)
			{
				return ViewportGizmoHandle::MoveFree;
			}

			if (!layout.isVerticalOnly)
			{
				const ImVec2 xTipPx(centerPx.x + kGizmoAxisLengthPx, centerPx.y);
				const bool nearXTip = distPx(mousePx, xTipPx) <= kGizmoHandleHitRadiusPx;
				const bool onXStem = (mousePx.x >= centerPx.x + 10.0f && mousePx.x <= xTipPx.x &&
									  std::abs(mousePx.y - centerPx.y) <= 5.5f);
				if (nearXTip || onXStem)
				{
					return ViewportGizmoHandle::MoveX;
				}
			}

			const ImVec2 yTipPx(centerPx.x, centerPx.y - kGizmoAxisLengthPx);
			const bool nearYTip = distPx(mousePx, yTipPx) <= kGizmoHandleHitRadiusPx;
			const bool onYStem =
				(mousePx.y <= centerPx.y - 10.0f && mousePx.y >= yTipPx.y && std::abs(mousePx.x - centerPx.x) <= 5.5f);
			if (nearYTip || onYStem)
			{
				return ViewportGizmoHandle::MoveY;
			}

			return ViewportGizmoHandle::None;
		}

		vec2 rotateLocalToWorld(vec2 center, vec2 local, float angle)
		{
			if (angle == 0.0f)
			{
				return center + local;
			}
			const float c = std::cos(angle);
			const float s = std::sin(angle);
			return vec2(center.x + c * local.x + s * local.y, center.y - s * local.x + c * local.y);
		}

		template <typename ToImGuiFn>
		void drawRotatedRoundedBoxOutline(ImDrawList* dl, ToImGuiFn&& toImGui, vec2 center, vec2 halfSize, float angle,
										  float radius, ImU32 col)
		{
			const float hx = std::abs(halfSize.x);
			const float hy = std::abs(halfSize.y);
			const float r = std::clamp(radius, 0.0f, (std::min)(hx, hy));

			if (r <= 1e-3f)
			{
				const vec2 local[4] = {{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};
				std::array<ImVec2, 4> pts{};
				for (int i = 0; i < 4; ++i)
				{
					pts[i] = toImGui(rotateLocalToWorld(center, local[i], angle));
				}
				dl->AddPolyline(pts.data(), 4, col, ImDrawFlags_Closed, 1.5f);
				return;
			}

			constexpr int kArcSteps = 4;
			std::array<ImVec2, 4 * (kArcSteps + 1)> pts{};
			const vec2 cornerCenters[4] = {
				{hx - r, hy - r},
				{-(hx - r), hy - r},
				{-(hx - r), -(hy - r)},
				{hx - r, -(hy - r)},
			};
			constexpr float kHalfPi = 1.57079632679f;
			int idx = 0;
			for (int c = 0; c < 4; ++c)
			{
				const float startA = static_cast<float>(c) * kHalfPi;
				for (int step = 0; step <= kArcSteps; ++step)
				{
					const float a = startA + (kHalfPi * static_cast<float>(step) / static_cast<float>(kArcSteps));
					const vec2 local = cornerCenters[c] + vec2(std::cos(a) * r, std::sin(a) * r);
					pts[idx++] = toImGui(rotateLocalToWorld(center, local, angle));
				}
			}
			dl->AddPolyline(pts.data(), idx, col, ImDrawFlags_Closed, 1.5f);
		}

		template <size_t N, typename ToImGuiFn>
		void drawRotatedPolygonOutline(ImDrawList* dl, ToImGuiFn&& toImGui, vec2 center,
									   const std::array<vec2, N>& localVerts, float angle, ImU32 col)
		{
			std::array<ImVec2, N> pts{};
			for (size_t i = 0; i < N; ++i)
			{
				pts[i] = toImGui(rotateLocalToWorld(center, localVerts[i], angle));
			}
			dl->AddPolyline(pts.data(), static_cast<int>(N), col, ImDrawFlags_Closed, 1.5f);
		}

		void drawHandleCircle(ImDrawList* dl, ImVec2 centerPx, float radiusPx, ImU32 ringCol, ImU32 fillCol)
		{
			dl->AddCircleFilled(centerPx, radiusPx, fillCol);
			dl->AddCircle(centerPx, radiusPx, ringCol, 0, 2.0f);
		}
	} // namespace
	void ViewportInteraction::handleHotkeys(EditorContext& ctx, SceneOperations& sceneOps)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantCaptureKeyboard || io.WantTextInput || ctx.unsavedPrompt.isOpenOrPending() ||
			ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
		{
			return;
		}

		const bool ctrlHeld = io.KeyCtrl || ctx.services->input().getKey(Input::LeftCtrl);
		const bool altHeld = io.KeyAlt;

		if (ctx.services->input().getKeyDown(Input::Esc))
		{
			if (ctx.isBoxSelecting)
			{
				ctx.selectedGroupId = 0;
				ctx.selectedEntities = ctx.boxSelectBaseEntities;
				ctx.selectedClusterIds = ctx.boxSelectBaseClusterIds;
				ctx.selectedEntity = ctx.selectedEntities.empty() ? INVALID_ENTITY : *ctx.selectedEntities.begin();
				ctx.selectedClusterId = ctx.selectedClusterIds.empty() ? 0 : *ctx.selectedClusterIds.begin();
				ctx.clearBoxSelectState();
			}
			else
			{
				sceneOps.deselectAll();
			}
		}

		if (ctrlHeld || altHeld)
		{
			return;
		}

		if (ctx.services->input().getKeyDown(Input::Space))
		{
			sceneOps.toggleSimulation();
		}
		if (!ctx.isSimulating && !ctx.isDragging && !ctx.isBoxSelecting && ctx.services->input().getKeyDown(Input::X))
		{
			sceneOps.deleteSelected();
		}
		if (ctx.services->input().getKeyDown(Input::F))
		{
			resetCamera(ctx);
		}
		if (ctx.services->input().getKeyDown(Input::C))
		{
			ctx.selectIndividualClusterEntities = !ctx.selectIndividualClusterEntities;
		}
	}

	void ViewportInteraction::handleCustomUIShortcuts(EditorContext& ctx, SceneOperations& sceneOps)
	{
		const bool modalBlocking =
			ctx.unsavedPrompt.isOpenOrPending() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
		if (modalBlocking)
		{
			return;
		}

		const bool allowUndo = !ctx.isSimulating && !ctx.isDragging && !ctx.isBoxSelecting &&
							   !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left);
		switch (Tools::pollUndoShortcut(allowUndo))
		{
			case Tools::UndoShortcutAction::Undo:
				sceneOps.undo();
				return;
			case Tools::UndoShortcutAction::Redo:
				sceneOps.redo();
				return;
			case Tools::UndoShortcutAction::None:
				break;
		}

		const ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput || ImGui::IsAnyItemActive())
		{
			return;
		}

		if (!io.KeyCtrl && !io.KeyAlt && !io.KeyShift)
		{
			if (!ctx.isSimulating && !ctx.isDragging && !ctx.isBoxSelecting &&
				(ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)))
			{
				sceneOps.deleteSelected();
			}
			return;
		}

		if (!io.KeyCtrl || io.KeyAlt)
		{
			return;
		}

		if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N, false))
		{
			sceneOps.requestNewScene();
		}
		else if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_O, false))
		{
			sceneOps.requestOpenLoadSceneDialog();
		}
		else if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
		{
			sceneOps.saveScenePrompt();
		}
		else if (!ctx.isSimulating && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_G, false))
		{
			sceneOps.ungroupSelected();
		}
		else if (!ctx.isSimulating && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_G, false))
		{
			sceneOps.groupSelected();
		}
		else if (!ctx.isSimulating && !io.KeyShift && !ctx.isDragging && !ctx.isBoxSelecting &&
				 ImGui::IsKeyPressed(ImGuiKey_D, false))
		{
			sceneOps.duplicateSelected();
		}
		else if (!ctx.isSimulating && !io.KeyShift && !ctx.isDragging && !ctx.isBoxSelecting &&
				 ImGui::IsKeyPressed(ImGuiKey_C, false))
		{
			sceneOps.copySelected();
		}
		else if (!ctx.isSimulating && !io.KeyShift && !ctx.isDragging && !ctx.isBoxSelecting &&
				 ImGui::IsKeyPressed(ImGuiKey_X, false))
		{
			sceneOps.cutSelected();
		}
		else if (!ctx.isSimulating && !io.KeyShift && !ctx.isDragging && !ctx.isBoxSelecting &&
				 ImGui::IsKeyPressed(ImGuiKey_V, false))
		{
			sceneOps.pasteClipboard(nullptr);
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
		if (ctx.registry == nullptr || ctx.services == nullptr)
		{
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		const bool modalBlocking = !ctx.project.isOpen() || ctx.unsavedPrompt.isOpenOrPending() ||
								   ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
		if (!ctx.isDragging && !ctx.isBoxSelecting && (io.WantCaptureMouse || modalBlocking))
		{
			ctx.hoveredGizmoHandle = ViewportGizmoHandle::None;
			return;
		}

		const Entity camEntity = ctx.services->render().getCameraEntity();
		if (!ctx.registry->hasComponent<Transform>(camEntity))
		{
			return;
		}

		const vec2 mouseWorld = getMouseWorld(ctx);
		const auto& cam = ctx.registry->getComponent<Transform>(camEntity);
		const float winH = static_cast<float>((std::max)(1, WeirdRenderer::Display::height));
		const float halfResY = (std::max)(1.0f, winH * 0.5f);
		const float worldPerPixel = (std::max)(1e-5f, std::abs(cam.position.z) / halfResY);

		if (!ctx.isDragging && !ctx.isBoxSelecting && !io.WantCaptureMouse && !modalBlocking)
		{
			const GizmoLayout hoverLayout = computeActiveGizmoLayout(ctx, sceneOps, cam);
			ctx.hoveredGizmoHandle = hitTestGizmoHandle(hoverLayout, cam, winH, mouseWorld);
		}
		else if (!ctx.isDragging)
		{
			ctx.hoveredGizmoHandle = ViewportGizmoHandle::None;
		}

		auto isHitInActiveSelection = [&](const WorldPickResult& hit) -> bool
		{
			if (hit.kind == WorldPickResult::Kind::Cluster)
			{
				if (ctx.isClusterSelected(hit.clusterId))
				{
					return true;
				}
				if (ctx.selectedGroupId != 0)
				{
					if (const auto* g = ctx.findGroup(ctx.selectedGroupId))
					{
						return std::find(g->clusterIds.begin(), g->clusterIds.end(), hit.clusterId) !=
							   g->clusterIds.end();
					}
				}
				return false;
			}
			if (hit.kind == WorldPickResult::Kind::Entity)
			{
				if (ctx.isEntitySelected(hit.entity))
				{
					return true;
				}
				if (ctx.selectedGroupId != 0)
				{
					if (const auto* g = ctx.findGroup(ctx.selectedGroupId))
					{
						if (std::find(g->entities.begin(), g->entities.end(), hit.entity) != g->entities.end())
						{
							return true;
						}
						if (const auto* owner = ctx.services->clusters().getClusterForEntity(hit.entity))
						{
							return std::find(g->clusterIds.begin(), g->clusterIds.end(), owner->id) !=
								   g->clusterIds.end();
						}
					}
				}
			}
			return false;
		};

		auto beginSelectionDrag = [&](ViewportGizmoHandle handleKind)
		{
			std::unordered_set<Entity> dragEntities;
			std::unordered_set<uint32_t> dragClusters;
			gatherEffectiveSelection(ctx, dragEntities, dragClusters);

			for (uint32_t cId : dragClusters)
			{
				if (auto* c = ctx.services->clusters().getById(cId))
				{
					ctx.dragClusterStartPositions[cId] = c->position;
					ctx.dragClusterOffsets[cId] = c->position - mouseWorld;
				}
			}

			for (Entity e : dragEntities)
			{
				const vec2 startPos = sceneOps.getEntityPosition(e);
				ctx.dragEntityStartPositions[e] = startPos;
				ctx.dragEntityOffsets[e] = startPos - mouseWorld;
			}

			if (!ctx.dragEntityStartPositions.empty() || !ctx.dragClusterStartPositions.empty())
			{
				ctx.activeGizmoHandle = handleKind;
				ctx.gizmoTargetEntity = INVALID_ENTITY;
				ctx.isDragging = true;
				ctx.dragHasMoved = false;
				ctx.dragStartMouseWorld = mouseWorld;
				ctx.dragStartPos = sceneOps.getSelectedPosition();
			}
		};

		// Left click down: prioritize active gizmo handles, otherwise pick/toggle selection or box select
		if (!io.WantCaptureMouse && !modalBlocking && ctx.services->input().getMouseButtonDown(Input::LeftClick))
		{
			ctx.clearDragState();
			ctx.clearBoxSelectState();

			const bool ctrlOrShift = io.KeyCtrl || io.KeyShift || ctx.services->input().getKey(Input::LeftCtrl);
			ViewportGizmoHandle handleHit = ViewportGizmoHandle::None;
			GizmoLayout layout;
			if (!ctrlOrShift)
			{
				layout = computeActiveGizmoLayout(ctx, sceneOps, cam);
				handleHit = hitTestGizmoHandle(layout, cam, winH, mouseWorld);
			}

			if (handleHit == ViewportGizmoHandle::LineStart || handleHit == ViewportGizmoHandle::LineEnd)
			{
				ctx.activeGizmoHandle = handleHit;
				ctx.gizmoTargetEntity = layout.targetEntity;
				ctx.dragHandleStartPos =
					(handleHit == ViewportGizmoHandle::LineStart) ? layout.lineStartWorld : layout.lineEndWorld;
				ctx.dragHandleOffset = ctx.dragHandleStartPos - mouseWorld;
				ctx.dragStartMouseWorld = mouseWorld;
				ctx.isDragging = true;
				ctx.dragHasMoved = false;
			}
			else if (handleHit != ViewportGizmoHandle::None)
			{
				beginSelectionDrag(handleHit);
			}
			else
			{
				const WorldPickResult hit = sceneOps.hitTestAtWorld(mouseWorld, worldPerPixel);

				if (hit.kind == WorldPickResult::Kind::None)
				{
					ctx.isBoxSelecting = true;
					ctx.boxSelectStartWorld = mouseWorld;
					ctx.boxSelectCurrentWorld = mouseWorld;
					ctx.boxSelectBaseEntities.clear();
					ctx.boxSelectBaseClusterIds.clear();

					if (ctrlOrShift)
					{
						ctx.boxSelectBaseEntities = ctx.selectedEntities;
						if (ctx.selectedEntity != INVALID_ENTITY)
						{
							ctx.boxSelectBaseEntities.insert(ctx.selectedEntity);
						}
						ctx.boxSelectBaseClusterIds = ctx.selectedClusterIds;
						if (ctx.selectedClusterId != 0)
						{
							ctx.boxSelectBaseClusterIds.insert(ctx.selectedClusterId);
						}
						if (ctx.selectedGroupId != 0)
						{
							if (const auto* g = ctx.findGroup(ctx.selectedGroupId))
							{
								for (Entity ge : g->entities)
								{
									ctx.boxSelectBaseEntities.insert(ge);
								}
								for (uint32_t gc : g->clusterIds)
								{
									ctx.boxSelectBaseClusterIds.insert(gc);
								}
							}
						}
					}
					else
					{
						sceneOps.deselectAll();
					}
				}
				else if (ctrlOrShift)
				{
					if (hit.kind == WorldPickResult::Kind::Cluster)
					{
						sceneOps.selectCluster(hit.clusterId, true);
						ctx.shiftAnchorItem = {HierarchyItemType::Cluster, hit.clusterId};
						ctx.hasShiftAnchor = true;
					}
					else
					{
						sceneOps.selectEntity(hit.entity, true);
						ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(hit.entity)};
						ctx.hasShiftAnchor = true;
					}
				}
				else
				{
					if (!isHitInActiveSelection(hit))
					{
						if (hit.kind == WorldPickResult::Kind::Cluster)
						{
							sceneOps.selectCluster(hit.clusterId, false);
						}
						else
						{
							sceneOps.selectEntity(hit.entity, false);
						}
					}

					if (hit.kind == WorldPickResult::Kind::Cluster)
					{
						ctx.shiftAnchorItem = {HierarchyItemType::Cluster, hit.clusterId};
					}
					else
					{
						ctx.shiftAnchorItem = {HierarchyItemType::Entity, static_cast<uint32_t>(hit.entity)};
					}
					ctx.hasShiftAnchor = true;
					beginSelectionDrag(ViewportGizmoHandle::MoveFree);
				}
			}
		}

		// Left drag box select
		if (ctx.isBoxSelecting)
		{
			const bool leftHeld = ctx.services->input().getMouseButton(Input::LeftClick);
			ctx.boxSelectCurrentWorld = mouseWorld;
			const float dragDistWorld = glm::distance(ctx.boxSelectStartWorld, ctx.boxSelectCurrentWorld);
			const bool passedThreshold = dragDistWorld >= worldPerPixel * 4.0f;

			if (passedThreshold)
			{
				sceneOps.selectInWorldBox(glm::min(ctx.boxSelectStartWorld, ctx.boxSelectCurrentWorld),
										  glm::max(ctx.boxSelectStartWorld, ctx.boxSelectCurrentWorld),
										  ctx.boxSelectBaseEntities, ctx.boxSelectBaseClusterIds);
			}

			if (!leftHeld)
			{
				ctx.clearBoxSelectState();
			}
		}

		// Left drag: either individual line endpoint tweak or multi-item offset move (free or X/Y constrained)
		if (ctx.isDragging && ctx.services->input().getMouseButton(Input::LeftClick))
		{
			const vec2 rawDelta = mouseWorld - ctx.dragStartMouseWorld;
			if (ctx.activeGizmoHandle == ViewportGizmoHandle::LineStart ||
				ctx.activeGizmoHandle == ViewportGizmoHandle::LineEnd)
			{
				const float dragDistWorld = glm::length(rawDelta);
				if (!ctx.dragHasMoved && dragDistWorld >= worldPerPixel * 1.5f)
				{
					ctx.dragHasMoved = true;
				}
				if (ctx.dragHasMoved)
				{
					vec2 targetPos = mouseWorld + ctx.dragHandleOffset;
					if (ctx.gridSnap && ctx.gridSize > 0.001f)
					{
						targetPos.x = std::round(targetPos.x / ctx.gridSize) * ctx.gridSize;
						targetPos.y = std::round(targetPos.y / ctx.gridSize) * ctx.gridSize;
					}
					const int endpointIdx = (ctx.activeGizmoHandle == ViewportGizmoHandle::LineStart) ? 0 : 1;
					sceneOps.setLineEndpoint(ctx.gizmoTargetEntity, endpointIdx, targetPos);
					ctx.isSceneDirty = true;
				}
			}
			else
			{
				vec2 constrainedDelta = rawDelta;
				if (ctx.activeGizmoHandle == ViewportGizmoHandle::MoveX)
				{
					constrainedDelta.y = 0.0f;
				}
				else if (ctx.activeGizmoHandle == ViewportGizmoHandle::MoveY)
				{
					constrainedDelta.x = 0.0f;
				}

				const float dragDistWorld = glm::length(constrainedDelta);
				if (!ctx.dragHasMoved && dragDistWorld >= worldPerPixel * 2.0f)
				{
					for (auto& [cId, startPos] : ctx.dragClusterStartPositions)
					{
						if (auto* c = ctx.services->clusters().getById(cId))
						{
							sceneOps.resetClusterShapeAtCurrentCentroid(*c);
							startPos = c->position;
							ctx.dragClusterOffsets[cId] = c->position - ctx.dragStartMouseWorld;
						}
					}
					if (ctx.dragClusterStartPositions.empty() && ctx.selectedClusterId != 0)
					{
						if (auto* c = ctx.services->clusters().getById(ctx.selectedClusterId))
						{
							sceneOps.resetClusterShapeAtCurrentCentroid(*c);
							ctx.dragStartPos = c->position;
						}
					}
					ctx.dragHasMoved = true;
				}

				if (ctx.dragHasMoved)
				{
					if (ctx.gridSnap && ctx.gridSize > 0.001f)
					{
						if (ctx.activeGizmoHandle != ViewportGizmoHandle::MoveY)
						{
							constrainedDelta.x = std::round(constrainedDelta.x / ctx.gridSize) * ctx.gridSize;
						}
						if (ctx.activeGizmoHandle != ViewportGizmoHandle::MoveX)
						{
							constrainedDelta.y = std::round(constrainedDelta.y / ctx.gridSize) * ctx.gridSize;
						}
					}

					const vec2 effectiveMouseWorld = ctx.dragStartMouseWorld + constrainedDelta;
					for (const auto& [e, offset] : ctx.dragEntityOffsets)
					{
						if (e != INVALID_ENTITY && !ctx.registry->isEntityDestroyed(e))
						{
							sceneOps.setEntityPosition(e, effectiveMouseWorld + offset);
						}
					}
					for (const auto& [cId, offset] : ctx.dragClusterOffsets)
					{
						sceneOps.setClusterPosition(cId, effectiveMouseWorld + offset);
					}
					ctx.isSceneDirty = true;
				}
			}
		}

		if (ctx.isDragging && !ctx.services->input().getMouseButton(Input::LeftClick))
		{
			ctx.clearDragState();
		}

		// Right-click in viewport: open creation/edit context menu at clicked world position
		if (!io.WantCaptureMouse && !modalBlocking && ctx.services->input().getMouseButtonDown(Input::RightClick))
		{
			ctx.contextMenuWorldPos = mouseWorld;
			ctx.openViewportContextMenu = true;
		}
	}

	void ViewportInteraction::renderViewportOverlays(EditorContext& ctx, SceneOperations& sceneOps)
	{
		if (ctx.registry == nullptr || ctx.services == nullptr)
		{
			return;
		}

		const Entity camEntity = ctx.services->render().getCameraEntity();
		if (!ctx.registry->hasComponent<Transform>(camEntity))
		{
			return;
		}
		const auto& cam = ctx.registry->getComponent<Transform>(camEntity);
		if (cam.position.z == 0.0f)
		{
			return;
		}

		const ImVec2 dispSize = ImGui::GetIO().DisplaySize;
		const float winH = static_cast<float>((std::max)(1, WeirdRenderer::Display::height));
		const float clipMinX = EditorContext::LEFT_PANEL_WIDTH;
		const float clipMinY = ctx.menuBarHeight;
		const float clipMaxX = (std::max)(clipMinX + 1.0f, dispSize.x - EditorContext::RIGHT_PANEL_WIDTH);
		const float clipMaxY = (std::max)(clipMinY + 1.0f, dispSize.y - EditorContext::BOTTOM_BAR_HEIGHT);

		auto toImGui = [&](vec2 worldPos) -> ImVec2 { return worldToImGuiScreen(cam, winH, worldPos); };

		ImDrawList* dl = ImGui::GetBackgroundDrawList();
		dl->PushClipRect(ImVec2(clipMinX, clipMinY), ImVec2(clipMaxX, clipMaxY), true);

		std::unordered_set<Entity> drawEntities;
		std::unordered_set<uint32_t> drawClusters;
		gatherEffectiveSelection(ctx, drawEntities, drawClusters);

		const GizmoLayout layout = computeActiveGizmoLayout(ctx, sceneOps, cam);
		const ImU32 entityOutlineCol = IM_COL32(95, 200, 255, 225);
		const ImU32 clusterOutlineCol = IM_COL32(255, 190, 70, 235);
		const float timeNow = ctx.services->time().time();

		for (Entity e : drawEntities)
		{
			if (ctx.registry->hasComponent<Shape>(e))
			{
				const auto& s = ctx.registry->getComponent<Shape>(e);
				const auto drawTriangleShape = [&](vec2 center, float w, float h, float angle, float expand)
				{
					const float hSign = (h >= 0.0f) ? 1.0f : -1.0f;
					const float hw = std::abs(w) * 0.5f + expand;
					const float bot = -h / 3.0f - hSign * expand * 0.85f;
					const float top = (2.0f * h) / 3.0f + hSign * expand * 1.15f;
					const std::array<vec2, 3> verts = {vec2(-hw, bot), vec2(hw, bot), vec2(0.0f, top)};
					drawRotatedPolygonOutline(dl, toImGui, center, verts, angle, entityOutlineCol);
				};

				switch (s.distanceFieldId)
				{
					case DefaultShapes::Circle:
					{
						const vec2 c(s.parameters[DefaultShapes::Circle::PosX],
									 s.parameters[DefaultShapes::Circle::PosY]);
						const float extra = (std::max)(0.0f, s.parameters[DefaultShapes::Circle::OutlineThickness]);
						const float r = std::abs(s.parameters[DefaultShapes::Circle::Radius]) + extra;
						const ImVec2 centerPx = toImGui(c);
						const ImVec2 edgePx = toImGui(c + vec2(r, 0.0f));
						const float radPx = (std::max)(2.0f, std::abs(edgePx.x - centerPx.x));
						dl->AddCircle(centerPx, radPx + 2.0f, entityOutlineCol, 0, 1.5f);
						continue;
					}
					case DefaultShapes::Box:
					{
						const vec2 c(s.parameters[DefaultShapes::Box::PosX], s.parameters[DefaultShapes::Box::PosY]);
						const vec2 halfSize(s.parameters[DefaultShapes::Box::SizeX],
											s.parameters[DefaultShapes::Box::SizeY]);
						const float cornerR = s.parameters[DefaultShapes::Box::Radius];
						const float angle = s.parameters[DefaultShapes::Box::Angle];
						const float outline = (std::max)(0.0f, s.parameters[DefaultShapes::Box::OutlineThickness]);
						drawRotatedRoundedBoxOutline(dl, toImGui, c, halfSize + vec2(outline), angle, cornerR + outline,
													 entityOutlineCol);
						continue;
					}
					case DefaultShapes::Triangle:
					{
						const float outline = (std::max)(0.0f, s.parameters[DefaultShapes::Triangle::OutlineThickness]);
						drawTriangleShape(vec2(s.parameters[DefaultShapes::Triangle::PosX],
											   s.parameters[DefaultShapes::Triangle::PosY]),
										  s.parameters[DefaultShapes::Triangle::Width],
										  s.parameters[DefaultShapes::Triangle::Height],
										  s.parameters[DefaultShapes::Triangle::Angle], outline);
						continue;
					}
					case DefaultShapes::Ramp:
					{
						const vec2 c(s.parameters[DefaultShapes::Ramp::PosX], s.parameters[DefaultShapes::Ramp::PosY]);
						const float wi = s.parameters[DefaultShapes::Ramp::Width];
						const float he = s.parameters[DefaultShapes::Ramp::Height];
						const float sk = s.parameters[DefaultShapes::Ramp::Skew];
						const float angle = s.parameters[DefaultShapes::Ramp::Angle];
						const std::array<vec2, 4> verts = {
							vec2(-wi, -sk - he),
							vec2(wi, sk - he),
							vec2(wi, sk + he),
							vec2(-wi, -sk + he),
						};
						drawRotatedPolygonOutline(dl, toImGui, c, verts, angle, entityOutlineCol);
						continue;
					}
					case DefaultShapes::Line:
					{
						const ImVec2 p0 = toImGui(
							vec2(s.parameters[DefaultShapes::Line::StartX], s.parameters[DefaultShapes::Line::StartY]));
						const ImVec2 p1 = toImGui(
							vec2(s.parameters[DefaultShapes::Line::EndX], s.parameters[DefaultShapes::Line::EndY]));
						dl->AddLine(p0, p1, entityOutlineCol, 2.0f);
						if (!layout.isLine)
						{
							dl->AddCircleFilled(p0, 3.5f, entityOutlineCol);
							dl->AddCircleFilled(p1, 3.5f, entityOutlineCol);
						}
						continue;
					}
					case DefaultShapes::SineWave:
					{
						const float yOffset = s.parameters[DefaultShapes::SineWave::Offset];
						const ImVec2 pCenter = toImGui(vec2(cam.position.x, yOffset));
						dl->AddLine(ImVec2(clipMinX, pCenter.y), ImVec2(clipMaxX, pCenter.y), entityOutlineCol, 1.5f);
						continue;
					}
					case DefaultShapes::Star:
					{
						const vec2 c(s.parameters[DefaultShapes::Star::PosX], s.parameters[DefaultShapes::Star::PosY]);
						const float r = s.parameters[DefaultShapes::Star::Radius];
						const float disp = s.parameters[DefaultShapes::Star::Displacement];
						const float points = s.parameters[DefaultShapes::Star::Points];
						const float speed = s.parameters[DefaultShapes::Star::Speed];
						constexpr int kStarSteps = 64;
						std::array<ImVec2, kStarSteps> pts{};
						for (int i = 0; i < kStarSteps; ++i)
						{
							const float theta = kTwoPi * (static_cast<float>(i) / static_cast<float>(kStarSteps));
							const float rad = (std::max)(0.05f, r - disp * std::sin(points * theta - speed * timeNow));
							pts[i] = toImGui(c + vec2(std::cos(theta) * rad, std::sin(theta) * rad));
						}
						dl->AddPolyline(pts.data(), kStarSteps, entityOutlineCol, ImDrawFlags_Closed, 1.5f);
						continue;
					}
					default:
						break;
				}
			}

			vec2 bMin(0.0f);
			vec2 bMax(0.0f);
			if (sceneOps.getEntityWorldBounds(e, bMin, bMax))
			{
				const ImVec2 pA = toImGui(bMin);
				const ImVec2 pB = toImGui(bMax);
				const ImVec2 rMin((std::min)(pA.x, pB.x) - 2.0f, (std::min)(pA.y, pB.y) - 2.0f);
				const ImVec2 rMax((std::max)(pA.x, pB.x) + 2.0f, (std::max)(pA.y, pB.y) + 2.0f);
				dl->AddRect(rMin, rMax, entityOutlineCol, 0.0f, 0, 1.5f);
			}
			else
			{
				const ImVec2 pt = toImGui(sceneOps.getEntityPosition(e));
				dl->AddRect(ImVec2(pt.x - 6.0f, pt.y - 6.0f), ImVec2(pt.x + 6.0f, pt.y + 6.0f), entityOutlineCol, 0.0f,
							0, 1.5f);
			}
		}

		for (uint32_t cId : drawClusters)
		{
			const auto* cluster = ctx.services->clusters().getById(cId);
			if (!cluster)
			{
				continue;
			}
			vec2 uMin(std::numeric_limits<float>::max());
			vec2 uMax(-std::numeric_limits<float>::max());
			bool hasBounds = false;
			for (Entity member : cluster->entities)
			{
				vec2 mMin(0.0f);
				vec2 mMax(0.0f);
				if (sceneOps.getEntityWorldBounds(member, mMin, mMax))
				{
					uMin = glm::min(uMin, mMin);
					uMax = glm::max(uMax, mMax);
					hasBounds = true;
				}
			}
			if (hasBounds)
			{
				const ImVec2 pA = toImGui(uMin);
				const ImVec2 pB = toImGui(uMax);
				const ImVec2 rMin((std::min)(pA.x, pB.x) - 4.0f, (std::min)(pA.y, pB.y) - 4.0f);
				const ImVec2 rMax((std::max)(pA.x, pB.x) + 4.0f, (std::max)(pA.y, pB.y) + 4.0f);
				dl->AddRect(rMin, rMax, clusterOutlineCol, 2.0f, 0, 1.75f);
			}
		}

		// Render interactive position gizmo handles on active selection
		if (layout.visible && !ctx.isBoxSelecting)
		{
			const ViewportGizmoHandle highlight = ctx.isDragging ? ctx.activeGizmoHandle : ctx.hoveredGizmoHandle;
			const ImU32 darkFill = IM_COL32(20, 26, 34, 225);
			const ImU32 hotRing = IM_COL32(255, 235, 110, 255);

			if (layout.isLine)
			{
				const ImVec2 startPx = toImGui(layout.lineStartWorld);
				const ImVec2 endPx = toImGui(layout.lineEndWorld);
				const bool startHot = (highlight == ViewportGizmoHandle::LineStart);
				const bool endHot = (highlight == ViewportGizmoHandle::LineEnd);

				drawHandleCircle(dl, startPx, startHot ? kGizmoHandleHoverRadiusPx : kGizmoHandleDrawRadiusPx,
								 startHot ? hotRing : IM_COL32(95, 215, 255, 245), darkFill);
				drawHandleCircle(dl, endPx, endHot ? kGizmoHandleHoverRadiusPx : kGizmoHandleDrawRadiusPx,
								 endHot ? hotRing : IM_COL32(95, 215, 255, 245), darkFill);
			}
			else
			{
				const ImVec2 centerPx = toImGui(layout.centerWorld);
				const bool freeHot = (highlight == ViewportGizmoHandle::MoveFree);
				const bool xHot = (highlight == ViewportGizmoHandle::MoveX);
				const bool yHot = (highlight == ViewportGizmoHandle::MoveY);

				if (ctx.isDragging && ctx.activeGizmoHandle == ViewportGizmoHandle::MoveX)
				{
					dl->AddLine(ImVec2(clipMinX, centerPx.y), ImVec2(clipMaxX, centerPx.y), IM_COL32(235, 90, 90, 95),
								1.0f);
				}
				if (ctx.isDragging && ctx.activeGizmoHandle == ViewportGizmoHandle::MoveY)
				{
					dl->AddLine(ImVec2(centerPx.x, clipMinY), ImVec2(centerPx.x, clipMaxY), IM_COL32(95, 225, 120, 95),
								1.0f);
				}

				if (!layout.isVerticalOnly)
				{
					const ImVec2 xTipPx(centerPx.x + kGizmoAxisLengthPx, centerPx.y);
					const ImU32 xCol = xHot ? IM_COL32(255, 145, 130, 255) : IM_COL32(235, 85, 85, 235);
					dl->AddLine(centerPx, xTipPx, xCol, xHot ? 3.0f : 2.0f);
					drawHandleCircle(dl, xTipPx, xHot ? kGizmoHandleHoverRadiusPx : kGizmoHandleDrawRadiusPx,
									 xHot ? hotRing : xCol, darkFill);
				}

				const ImVec2 yTipPx(centerPx.x, centerPx.y - kGizmoAxisLengthPx);
				const ImU32 yCol = yHot ? IM_COL32(150, 255, 165, 255) : IM_COL32(95, 220, 115, 235);
				dl->AddLine(centerPx, yTipPx, yCol, yHot ? 3.0f : 2.0f);
				drawHandleCircle(dl, yTipPx, yHot ? kGizmoHandleHoverRadiusPx : kGizmoHandleDrawRadiusPx,
								 yHot ? hotRing : yCol, darkFill);

				const ImU32 centerCol =
					freeHot ? hotRing
							: (!drawClusters.empty() && drawEntities.empty() ? clusterOutlineCol
																			 : IM_COL32(240, 248, 255, 245));
				drawHandleCircle(dl, centerPx, freeHot ? kGizmoHandleHoverRadiusPx : kGizmoHandleDrawRadiusPx,
								 centerCol, darkFill);
			}
		}

		if (ctx.isBoxSelecting)
		{
			const ImVec2 p0 = toImGui(ctx.boxSelectStartWorld);
			const ImVec2 p1 = toImGui(ctx.boxSelectCurrentWorld);
			if (std::hypot(p1.x - p0.x, p1.y - p0.y) >= 4.0f)
			{
				const ImVec2 rMin((std::min)(p0.x, p1.x), (std::min)(p0.y, p1.y));
				const ImVec2 rMax((std::max)(p0.x, p1.x), (std::max)(p0.y, p1.y));
				dl->AddRectFilled(rMin, rMax, IM_COL32(80, 165, 255, 42));
				dl->AddRect(rMin, rMax, IM_COL32(105, 190, 255, 230), 0.0f, 0, 1.5f);
			}
		}

		dl->PopClipRect();
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

			if (!ctx.isSimulating && (ctx.hasSelection() || sceneOps.canPaste()))
			{
				if (ImGui::MenuItem("Cut", "Ctrl+X", false, ctx.hasSelection()))
				{
					sceneOps.cutSelected();
				}
				if (ImGui::MenuItem("Copy", "Ctrl+C", false, ctx.hasSelection()))
				{
					sceneOps.copySelected();
				}
				if (ImGui::MenuItem("Paste Here", "Ctrl+V", false, sceneOps.canPaste()))
				{
					sceneOps.pasteClipboard(&spawnPos);
				}
				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, ctx.hasSelection()))
				{
					sceneOps.duplicateSelected();
				}
				if (ImGui::MenuItem("Delete", "Del / X", false, ctx.hasSelection()))
				{
					sceneOps.deleteSelected();
				}
				ImGui::Separator();
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
				if (ImGui::MenuItem("SineWave"))
				{
					sceneOps.spawnDefaultShape(DefaultShapes::SineWave, spawnPos);
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

	void ViewportInteraction::renderProjectModal(EditorContext& ctx, SceneOperations& sceneOps)
	{
		if (ctx.project.isOpen())
		{
			return;
		}

		const float winW = ImGui::GetIO().DisplaySize.x;
		const float winH = ImGui::GetIO().DisplaySize.y;

		ImGui::SetNextWindowSize(ImVec2(450.0f, 325.0f), ImGuiCond_Always);
		ImGui::SetNextWindowPos(ImVec2(winW * 0.5f - 225.0f, winH * 0.5f - 162.5f), ImGuiCond_Always);

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

			const auto recentProjects = ctx.project.getRecentProjects();
			if (recentProjects.empty())
			{
				ImGui::TextDisabled("No recent projects found.");
			}
			else if (ImGui::BeginChild("##RecentProjectsModalList", ImVec2(0.0f, 96.0f), ImGuiChildFlags_Borders))
			{
				for (size_t i = 0; i < recentProjects.size(); ++i)
				{
					const std::string& p = recentProjects[i];
					std::string name = std::filesystem::path(p).filename().string();
					if (name.empty())
					{
						name = p;
					}
					std::string label = name + "##ModalProj" + std::to_string(i);
					if (ImGui::Selectable(label.c_str()))
					{
						ctx.project.open(p);
					}
					if (ImGui::IsItemHovered() && ImGui::BeginTooltip())
					{
						ImGui::TextUnformatted(p.c_str());
						ImGui::EndTooltip();
					}
				}
				ImGui::EndChild();
			}

			if (ctx.services != nullptr)
			{
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				if (ImGui::Button("Back to Main Menu", ImVec2(-1.0f, 0.0f)))
				{
					sceneOps.confirmIfDirty([&ctx]()
											{ ToolsSession::getInstance().navigateToMainMenu(*ctx.services); });
				}
			}

			ImGui::End();
		}
	}
} // namespace WeirdEngine::Tools::SceneEd
