#pragma once

#include <string>
#include <weird-engine.h>

#include "shape-editor/model/NodeGraph.h"
#include "shape-editor/services/AudioPreviewManager.h"
#include "shape-editor/services/PreviewController.h"

namespace WeirdEngine::Tools::ShapeEd
{
	enum class InspectorTab
	{
		Parameters,
		Audio,
		CodeShader
	};

	class InspectorView
	{
	public:
		InspectorView() = default;

		void render(ServiceProvider& services, NodeGraph& graph, PreviewController& previewController,
					AudioPreviewManager& audioManager, const std::string& cachedGlslCode,
					const std::string& cachedCppCode, bool* outGraphEdited = nullptr);

		InspectorTab getActiveTab() const;
		void setActiveTab(InspectorTab tab);

	private:
		void renderParametersTab(ServiceProvider& services, NodeGraph& graph, PreviewController& previewController,
								 AudioPreviewManager& audioManager, bool* outGraphEdited);
		void renderAudioTab(ServiceProvider& services, AudioPreviewManager& audioManager);
		void renderCodeShaderTab(const std::string& cachedGlslCode, const std::string& cachedCppCode);

		InspectorTab m_activeTab = InspectorTab::Parameters;
	};
} // namespace WeirdEngine::Tools::ShapeEd
