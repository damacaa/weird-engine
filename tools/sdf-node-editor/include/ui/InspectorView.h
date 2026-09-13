#pragma once

#include <string>
#include <weird-engine.h>

#include "model/NodeGraph.h"
#include "services/AudioPreviewManager.h"
#include "services/PreviewController.h"

namespace WeirdEngine::Editor
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
					const std::string& cachedCppCode);

		InspectorTab getActiveTab() const;
		void setActiveTab(InspectorTab tab);

	private:
		void renderParametersTab(ServiceProvider& services, NodeGraph& graph, PreviewController& previewController,
								 AudioPreviewManager& audioManager);
		void renderAudioTab(ServiceProvider& services, AudioPreviewManager& audioManager);
		void renderCodeShaderTab(const std::string& cachedGlslCode, const std::string& cachedCppCode);

		InspectorTab m_activeTab = InspectorTab::Parameters;
	};
} // namespace WeirdEngine::Editor
