#include "ui/InspectorView.h"

#include "weird-renderer/audio/AudioEngine.h"
#include <cstdio>
#include <imgui.h>

namespace WeirdEngine::Editor
{
	InspectorTab InspectorView::getActiveTab() const
	{
		return m_activeTab;
	}

	void InspectorView::setActiveTab(InspectorTab tab)
	{
		m_activeTab = tab;
	}

	void InspectorView::render(ServiceProvider& services, NodeGraph& graph, PreviewController& previewController,
							   AudioPreviewManager& audioManager, const std::string& cachedGlslCode,
							   const std::string& cachedCppCode)
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
				renderParametersTab(services, graph, previewController, audioManager);
				ImGui::EndTabItem();
			}

			// =============================================================
			// Tab 2: Procedural Music & Audio
			// =============================================================
			if (ImGui::BeginTabItem("Audio & Song"))
			{
				selectedTab = InspectorTab::Audio;
				renderAudioTab(services, audioManager);
				ImGui::EndTabItem();
			}

			// =============================================================
			// Tab 3: Shader & Code Exporter
			// =============================================================
			if (ImGui::BeginTabItem("Code & Shader"))
			{
				selectedTab = InspectorTab::CodeShader;
				renderCodeShaderTab(cachedGlslCode, cachedCppCode);
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();

			if (selectedTab != m_activeTab)
			{
				if (m_activeTab == InspectorTab::Parameters && selectedTab != InspectorTab::Parameters)
				{
					previewController.clearSpawnedDots(services.registry());
				}
				m_activeTab = selectedTab;
			}
		}
	}

	void InspectorView::renderParametersTab(ServiceProvider& services, NodeGraph& graph,
											PreviewController& previewController, AudioPreviewManager& audioManager)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "World Shape Parameters (var0 .. var7)");
		ImGui::TextDisabled("Dynamic parameters bound to CustomShape (var0..var7).");
		ImGui::Separator();
		ImGui::Spacing();

		auto& params = graph.getParameters();
		for (int i = 0; i < 8; ++i)
		{
			ImGui::PushID(i);

			char defaultHint[16];
			snprintf(defaultHint, sizeof(defaultHint), "var%d", i);

			char nameBuf[64];
			const std::string& currentName = graph.getParameterName(i);
			strncpy(nameBuf, currentName.c_str(), sizeof(nameBuf) - 1);
			nameBuf[sizeof(nameBuf) - 1] = '\0';

			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::InputTextWithHint("##name", defaultHint, nameBuf, sizeof(nameBuf)))
			{
				graph.setParameterName(i, nameBuf);
			}

			ImGui::SameLine(0.0f, 6.0f);
			ImGui::SetNextItemWidth(-1.0f);
			char idStr[32];
			snprintf(idStr, sizeof(idStr), "##var%d", i);
			if (ImGui::DragFloat(idStr, &params[i], 0.1f, -1000.0f, 1000.0f, "%.2f"))
			{
				previewController.syncParameters(services, params);
				if (auto song = audioManager.getSong())
				{
					for (size_t p = 0; p < 8; ++p)
					{
						song->setParameter(p, params[p]);
						services.audio().setSongParameter(p, params[p]);
					}
					song->calculateMusicalPropertiesFromShape();
				}
			}
			ImGui::PopID();
		}

		ImGui::Spacing();
		ImGui::TextWrapped("Tip: In your node graph, add 'Variable (var0..var7)' from the Input category to link "
						   "these variables to shape sizes, positions, or math nodes.");

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Physics Simulation", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::Button("Clear Spawned Dots", ImVec2(-1.0f, 22.0f)))
			{
				previewController.clearSpawnedDots(services.registry());
			}
			ImGui::TextDisabled("Click/drag inside the preview window to spawn physics dots!");
		}
	}

	void InspectorView::renderAudioTab(ServiceProvider& services, AudioPreviewManager& audioManager)
	{
		auto& music = services.audio().music();

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.9f, 1.0f), "Procedural SDF Music Synthesizer");
		ImGui::Separator();

		bool isPlaying = audioManager.isPlaying();
		if (ImGui::Checkbox("Play as Song (Real-Time)", &isPlaying))
		{
			audioManager.setPlaying(services, isPlaying);
		}

		float volume = audioManager.getVolume();
		if (ImGui::SliderFloat("Master Volume", &volume, 0.0f, 1.0f, "%.2f"))
		{
			audioManager.setVolume(services, volume);
		}

		auto song = audioManager.getSong();

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Musical Properties", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				ImGui::Text("Scale:  %s", AudioPreviewManager::getScaleName(song->getScale()));
				ImGui::Text("Tempo:  %.1f BPM", song->getTempo());
				ImGui::Text("Root:   %s (MIDI %d)", AudioPreviewManager::midiToNoteString(song->getRootMidi()).c_str(),
							song->getRootMidi());

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
		if (ImGui::CollapsingHeader("Instrument Rack (AST Driven)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const auto& rack = music.getInstrumentRack();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Assigned Waveforms & Kit:");

			bool leadEnabled = music.isTrackEnabled(WeirdRenderer::MusicTrack::Lead);
			if (ImGui::Checkbox("##lead_toggle", &leadEnabled))
			{
				music.setTrackEnabled(WeirdRenderer::MusicTrack::Lead, leadEnabled);
			}
			ImGui::SameLine();
			ImGui::Text("Lead Wave:   %s", AudioPreviewManager::getWaveTypeName(rack.lead));

			bool bassEnabled = music.isTrackEnabled(WeirdRenderer::MusicTrack::Bass);
			if (ImGui::Checkbox("##bass_toggle", &bassEnabled))
			{
				music.setTrackEnabled(WeirdRenderer::MusicTrack::Bass, bassEnabled);
			}
			ImGui::SameLine();
			ImGui::Text("Bass Wave:   %s", AudioPreviewManager::getWaveTypeName(rack.bass));

			bool padEnabled = music.isTrackEnabled(WeirdRenderer::MusicTrack::Pad);
			if (ImGui::Checkbox("##pad_toggle", &padEnabled))
			{
				music.setTrackEnabled(WeirdRenderer::MusicTrack::Pad, padEnabled);
			}
			ImGui::SameLine();
			ImGui::Text("Pad Wave:    %s", AudioPreviewManager::getWaveTypeName(rack.pad));

			bool drumsEnabled = music.isTrackEnabled(WeirdRenderer::MusicTrack::Drums);
			if (ImGui::Checkbox("##drums_toggle", &drumsEnabled))
			{
				music.setTrackEnabled(WeirdRenderer::MusicTrack::Drums, drumsEnabled);
			}
			ImGui::SameLine();
			ImGui::Text("Drum Kit:    %s", AudioPreviewManager::getDrumKitName(rack.drumKit));

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Synthesis Parameters:");
			ImGui::Text("Pulse Width: %.2f", rack.pulseWidth);
			ImGui::Text("FM Mod Index:%.2f", rack.fmModIndex);
			ImGui::Text("Fold Drive:  %.2f", rack.foldDrive);
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("AST Topology Fingerprint", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				const auto& fp = song->getFingerprint();
				ImGui::Text("Structural Seed: 0x%08X", fp.structuralHash);
				ImGui::Text("Total Operators: %d", fp.nodeCount);
				ImGui::Text("Tree Depth:      %d", fp.maxDepth);
				ImGui::Text("Branching Nodes: %d", fp.branchCount);
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Live Oscilloscope", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto audioData = WeirdRenderer::AudioEngine::getInstance().getAudioData();
			if (!audioData.waveform.empty())
			{
				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 0.9f, 0.8f, 1.0f));
				ImGui::PlotLines("##Waveform", audioData.waveform.data(), static_cast<int>(audioData.waveform.size()),
								 0, nullptr, -1.0f, 1.0f, ImVec2(-1.0f, 75.0f));
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

	void InspectorView::renderCodeShaderTab(const std::string& cachedGlslCode, const std::string& cachedCppCode)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Live GLSL AST Shader Output");
		ImGui::Separator();

		if (ImGui::Button("Copy GLSL Code", ImVec2(-1.0f, 25.0f)))
		{
			ImGui::SetClipboardText(cachedGlslCode.c_str());
		}

		ImGui::InputTextMultiline("##GlslCodeBox", const_cast<char*>(cachedGlslCode.data()), cachedGlslCode.size(),
								  ImVec2(-1.0f, 150.0f), ImGuiInputTextFlags_ReadOnly);

		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "C++ Expr Code Snippet");
		ImGui::Separator();

		if (ImGui::Button("Copy C++ Code", ImVec2(-1.0f, 25.0f)))
		{
			ImGui::SetClipboardText(cachedCppCode.c_str());
		}

		ImGui::InputTextMultiline("##CppCodeBox", const_cast<char*>(cachedCppCode.data()), cachedCppCode.size(),
								  ImVec2(-1.0f, 200.0f), ImGuiInputTextFlags_ReadOnly);
	}
} // namespace WeirdEngine::Editor
