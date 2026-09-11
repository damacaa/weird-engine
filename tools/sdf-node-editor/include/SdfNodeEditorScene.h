#pragma once

#include <algorithm>
#include <memory>
#include <string>

#include <imgui.h>
#include <imgui_internal.h>
#include <weird-engine.h>

#include "imnodes.h"
#include "model/NodeGraph.h"
#include "model/NodeGraphCompiler.h"
#include "model/NodeGraphPresets.h"
#include "model/NodeRegistry.h"
#include "services/AudioPreviewManager.h"
#include "services/EditorFileManager.h"
#include "services/PreviewController.h"
#include "ui/EditorTheme.h"
#include "ui/InspectorView.h"
#include "ui/MenuBarView.h"
#include "ui/NodeCanvasView.h"
#include "weird-renderer/core/Display.h"

namespace WeirdEngine::Editor
{
	class SdfNodeEditorScene : public Scene2D
	{
	public:
		SdfNodeEditorScene() = default;
		~SdfNodeEditorScene() override = default;

	protected:
		void onCreate(Registry& registry, ServiceProvider& services) override
		{
			m_previewController.initMaterials(services);
			NodeGraphPresets::loadStar(m_graph);
			m_dirty = true;
		}

		void onStart(Registry& registry, ServiceProvider& services) override
		{
			m_previewController.initCamera(registry, services);

			ImNodes::CreateContext();
			EditorTheme::setupImNodesStyle();

			m_graph.autoLayout();
			m_syncPositionsToImNodes = true;

			rebuildAndSync(services);
			m_audioManager.updateAudioState(services);
		}

		void onDestroy(Registry& registry, ServiceProvider& services) override
		{
			ImNodes::DestroyContext();
			m_previewController.destroyEntities(registry);
		}

		void onUpdate(Registry& registry, ServiceProvider& services) override
		{
			m_fileManager.pollFileActions(m_graph, m_dirty, m_syncPositionsToImNodes);

			if (m_dirty)
			{
				rebuildAndSync(services);
				m_dirty = false;
			}

			m_previewController.update(registry, services);
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

			m_evaluatedExpr = NodeGraphCompiler::evaluate(m_graph);
			if (!m_evaluatedExpr.node)
			{
				m_evaluatedExpr = Expr(1.0f);
			}

			m_previewController.syncPreviewShape(services.registry(), services, m_evaluatedExpr,
												 m_graph.getParameters());

			glm::vec2 uiCenter = (m_previewController.getPreviewMode() == PreviewMode::UIShape)
									 ? targetCenter
									 : glm::vec2(100000.0f, 100000.0f);
			m_audioManager.syncSong(services, m_evaluatedExpr, uiCenter, m_graph.getParameters());

			m_cachedGlslCode = m_evaluatedExpr.node ? m_evaluatedExpr.node->print() : "0.0";
			m_cachedCppCode = NodeGraphCompiler::generateCppCode(m_graph);
		}

		void renderEditorUI(ServiceProvider& services)
		{
			int winW = WeirdRenderer::Display::width > 0 ? WeirdRenderer::Display::width : 1280;
			int winH = WeirdRenderer::Display::height > 0 ? WeirdRenderer::Display::height : 800;

			ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)), ImGuiCond_Always);

			ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
									 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									 ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

			if (ImGui::Begin("##NodeEditorMainSpace", nullptr, flags))
			{
				ImGui::PopStyleVar(3);

				MenuBarView::render(m_graph, m_fileManager, m_dirty, m_syncPositionsToImNodes);

				float menuH = ImGui::GetFrameHeight();
				float contentH = static_cast<float>(winH) - menuH;

				float inspectorWidth = 360.0f;
				float canvasWidth = static_cast<float>(winW) - inspectorWidth;
				float previewHeight = std::min(360.0f, std::max(240.0f, contentH * 0.40f));
				float inspectorHeight = contentH - previewHeight;

				// 1. Interactive Node Editor Canvas
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				ImGui::BeginChild("##NodeCanvasChild", ImVec2(canvasWidth, contentH), false,
								  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				NodeCanvasView::render(m_graph, m_fileManager, m_dirty, m_syncPositionsToImNodes);
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();

				ImGui::SameLine(0.0f, 0.0f);

				// 2. Right Column: Inspector
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));

				ImGui::BeginChild("##InspectorChild", ImVec2(0.0f, inspectorHeight), true,
								  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				m_inspectorView.render(services, m_graph, m_previewController, m_audioManager, m_cachedGlslCode,
									   m_cachedCppCode);
				ImGui::EndChild();

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(2);

				// Frame the transparent preview gap with a border
				ImVec2 gapMin(canvasWidth, menuH + inspectorHeight);
				ImVec2 gapMax(static_cast<float>(winW), static_cast<float>(winH));
				ImGui::GetWindowDrawList()->AddRect(gapMin, gapMax, IM_COL32(55, 55, 55, 255));

				// Preview selector in preview area
				ImGui::SetNextWindowPos(ImVec2(gapMax.x - PreviewController::kPreviewSelectorWidth -
												   PreviewController::kPreviewSelectorRightMargin,
											   gapMin.y + PreviewController::kPreviewSelectorTopMargin),
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
					const bool worldSelected = m_previewController.getPreviewMode() == PreviewMode::WorldShape;
					if (worldSelected)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.65f, 1.0f));
					if (ImGui::Button("World", ImVec2(50.0f, 0.0f)))
					{
						if (!worldSelected)
						{
							m_previewController.setPreviewMode(services.registry(), services, PreviewMode::WorldShape,
															   m_evaluatedExpr, m_graph.getParameters());
							m_dirty = true;
						}
					}
					if (worldSelected)
						ImGui::PopStyleColor();

					ImGui::SameLine(0.0f, 4.0f);
					const bool uiSelected = m_previewController.getPreviewMode() == PreviewMode::UIShape;
					if (uiSelected)
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.55f, 0.65f, 1.0f));
					if (ImGui::Button("UI", ImVec2(50.0f, 0.0f)))
					{
						if (!uiSelected)
						{
							m_previewController.setPreviewMode(services.registry(), services, PreviewMode::UIShape,
															   m_evaluatedExpr, m_graph.getParameters());
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

		NodeGraph m_graph;
		EditorFileManager m_fileManager;
		PreviewController m_previewController;
		AudioPreviewManager m_audioManager;
		InspectorView m_inspectorView;

		Expr m_evaluatedExpr;
		std::string m_cachedGlslCode;
		std::string m_cachedCppCode;

		bool m_dirty = true;
		bool m_syncPositionsToImNodes = true;
	};
} // namespace WeirdEngine::Editor
