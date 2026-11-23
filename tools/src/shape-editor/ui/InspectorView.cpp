#include "shape-editor/ui/InspectorView.h"

#include "weird-audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>

namespace WeirdEngine::Tools::ShapeEd
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
							   const std::string& cachedCppCode, bool* outGraphEdited)
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
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				if (ImGui::BeginChild("##ParametersTabContent", ImVec2(0.0f, 0.0f), false))
				{
					renderParametersTab(services, graph, previewController, audioManager, outGraphEdited);
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::EndTabItem();
			}

			// =============================================================
			// Tab 2: Procedural Music & Audio
			// =============================================================
			if (ImGui::BeginTabItem("Audio & Song"))
			{
				selectedTab = InspectorTab::Audio;
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				if (ImGui::BeginChild("##AudioTabContent", ImVec2(0.0f, 0.0f), false))
				{
					renderAudioTab(services, audioManager);
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::EndTabItem();
			}

			// =============================================================
			// Tab 3: Shader & Code Exporter
			// =============================================================
			if (ImGui::BeginTabItem("Code & Shader"))
			{
				selectedTab = InspectorTab::CodeShader;
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				if (ImGui::BeginChild("##CodeShaderTabContent", ImVec2(0.0f, 0.0f), false))
				{
					renderCodeShaderTab(cachedGlslCode, cachedCppCode);
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
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
											PreviewController& previewController, AudioPreviewManager& audioManager,
											bool* outGraphEdited)
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "World Shape Parameters (var0 .. var7)");
		ImGui::TextDisabled("Dynamic parameters bound to Shape (var0..var7).");
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
			snprintf(nameBuf, sizeof(nameBuf), "%s", currentName.c_str());
			nameBuf[sizeof(nameBuf) - 1] = '\0';

			ImGui::SetNextItemWidth(90.0f);
			if (ImGui::InputTextWithHint("##name", defaultHint, nameBuf, sizeof(nameBuf)))
			{
				graph.setParameterName(i, nameBuf);
				if (outGraphEdited)
					*outGraphEdited = true;
			}

			ImGui::SameLine(0.0f, 6.0f);
			ImGui::SetNextItemWidth(-1.0f);
			char idStr[32];
			snprintf(idStr, sizeof(idStr), "##var%d", i);
			if (ImGui::DragFloat(idStr, &params[i], 0.1f, -1000.0f, 1000.0f, "%.2f"))
			{
				if (outGraphEdited)
					*outGraphEdited = true;
				previewController.syncParameters(services, params);
				if (auto song = audioManager.getSong())
				{
					song->setParameter(static_cast<size_t>(i), params[i]);
					services.audio().setSongParameter(static_cast<size_t>(i), params[i]);
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
		ImGui::SameLine();
		if (ImGui::SmallButton("Resample"))
		{
			if (auto currentSong = audioManager.getSong())
			{
				currentSong->calculateMusicalPropertiesFromShape();
			}
			services.audio().resampleShape();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Immediately re-probe domain motion, fill, curvature, and musical properties.");
		}

		float volume = audioManager.getVolume();
		if (ImGui::SliderFloat("Master Volume", &volume, 0.0f, 1.0f, "%.2f"))
		{
			audioManager.setVolume(services, volume);
		}

		auto song = audioManager.getSong();

		auto drawKeyValueRow = [](const char* label, const char* value, const char* tooltip = nullptr)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", label);
			if (tooltip && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tooltip);
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::TextWrapped("%s", value);
		};

		auto drawMetricBarRow =
			[](const char* label, float fraction, const char* overlay, const char* tooltip = nullptr)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(label);
			if (tooltip && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tooltip);
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::ProgressBar(std::clamp(fraction, 0.0f, 1.0f), ImVec2(-1.0f, 0.0f), overlay);
			if (tooltip && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", tooltip);
			}
		};

		auto beginInspectorTable = [](const char* id, float labelColWidth) -> bool
		{
			if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings))
			{
				return false;
			}
			ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, labelColWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			return true;
		};

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Musical Properties", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				if (beginInspectorTable("##MusicalPropsTable", 110.0f))
				{
					char buf[96];

					drawKeyValueRow("Scale", AudioPreviewManager::getScaleName(song->getScale()),
									"Derived from 24-angle perimeter profiling (smoothness, spikiness, hollowness, "
									"asymmetry).");

					const int rootMidi = song->getRootMidi();
					std::snprintf(buf, sizeof(buf), "%s (MIDI %d)",
								  AudioPreviewManager::midiToNoteString(rootMidi).c_str(), rootMidi);
					drawKeyValueRow("Root Key", buf,
									"Derived from center SDF depth, mean perimeter distance, and domain fill ratio.");

					std::snprintf(buf, sizeof(buf), "%.1f BPM", song->getTempo());
					drawKeyValueRow("Base Tempo", buf, "Static base tempo derived from domain motion probe.");

					std::snprintf(buf, sizeof(buf), "%.1f BPM (%.2fs)", music.getTempo(), music.getTimeBetweenBeats());
					drawKeyValueRow("Dynamic Tempo", buf,
									"Live tempo = Base Tempo x Tempo Factor x Motion Mult x Surge/Duck.");

					const auto leadStyle = music.getLeadStyle();
					const auto effectiveLeadStyle = music.getEffectiveLeadStyle();
					if (leadStyle == WeirdAudio::LeadStyle::Auto)
					{
						std::snprintf(buf, sizeof(buf), "%s (Auto)",
									  AudioPreviewManager::getLeadStyleName(effectiveLeadStyle));
					}
					else
					{
						std::snprintf(buf, sizeof(buf), "%s", AudioPreviewManager::getLeadStyleName(leadStyle));
					}
					drawKeyValueRow("Lead Style", buf,
									"Selected from motion, melody density, syncopation, and AST structural hash.");

					ImGui::EndTable();
				}
			}
			else
			{
				ImGui::TextDisabled("No active shape song.");
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Domain Sampling Metrics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.18f, 0.56f, 0.72f, 1.0f));
				if (beginInspectorTable("##DomainMetricsTable", 122.0f))
				{
					char overlay[64];
					char complexityTooltip[128];

					const float motionNorm = music.getMotionNorm();
					std::snprintf(overlay, sizeof(overlay), "%.2f (raw %.1f)", motionNorm, music.getMotionLevel());
					drawMetricBarRow("Motion Level", motionNorm, overlay,
									 "Boundary movement rate |d1 - d0| / dt averaged across 32 domain points.");

					const float tempoFromMotion = music.getTempoFromMotion();
					std::snprintf(overlay, sizeof(overlay), "%.2fx", tempoFromMotion);
					drawMetricBarRow("Motion Tempo", (tempoFromMotion - 0.45f) / (1.20f - 0.45f), overlay,
									 "Tempo multiplier derived from motion norm [0.45x .. 1.20x].");

					const float fillRatio = std::clamp(music.getFillRatio(), 0.0f, 1.0f);
					std::snprintf(overlay, sizeof(overlay), "%.0f%% (%.2f)", fillRatio * 100.0f, fillRatio);
					drawMetricBarRow("Domain Fill", fillRatio, overlay,
									 "Fraction of 32 domain sample points inside the shape (d < 0).");

					const float volumeFromFill = music.getVolumeFromFill();
					std::snprintf(overlay, sizeof(overlay), "%.2fx", volumeFromFill);
					drawMetricBarRow("Fill Volume", (volumeFromFill - 0.35f) / (1.00f - 0.35f), overlay,
									 "Volume multiplier derived from fill ratio [0.35x .. 1.00x].");

					const float complexityNorm = music.getComplexityNorm();
					std::snprintf(overlay, sizeof(overlay), "%.2f (raw %.2f)", complexityNorm,
								  music.getComplexityLevel());
					std::snprintf(complexityTooltip, sizeof(complexityTooltip),
								  "Scale-normalized curvature spread stddev(kappa) * R_char (saturation %.1f).",
								  music.getComplexitySaturation());
					drawMetricBarRow("Complexity", complexityNorm, overlay, complexityTooltip);

					const float tension = music.getTension();
					std::snprintf(overlay, sizeof(overlay), "%.2f", tension);
					drawMetricBarRow("Tension", tension, overlay,
									 "Complexity-derived tension metric (complexity ^ 0.8).");

					ImGui::EndTable();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::TextDisabled("No active shape song.");
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Visual Shape Traits", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				const auto traits = music.getVisualTraits();
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.60f, 0.48f, 1.0f));
				if (beginInspectorTable("##VisualTraitsTable", 122.0f))
				{
					char overlay[32];

					std::snprintf(overlay, sizeof(overlay), "%.2f", traits.mass);
					drawMetricBarRow("Size / Mass", traits.mass, overlay,
									 "Area fill & radial reach -> drives Harmony Richness & Bass Weight.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", traits.motion);
					drawMetricBarRow("Motion Speed", traits.motion, overlay,
									 "Boundary movement speed -> drives Tempo Factor & Percussion Energy.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", traits.spikiness);
					drawMetricBarRow("Spikiness / Sharp", traits.spikiness, overlay,
									 "Perimeter lobes & surface curvature -> drives Brightness, Melody Density, "
									 "Variation & Timbre.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", traits.hollowness);
					drawMetricBarRow("Hollowness", traits.hollowness, overlay,
									 "Center cutout / ring vs solid core -> drives Syncopation, Variation & Scale.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", traits.asymmetry);
					drawMetricBarRow("Asymmetry", traits.asymmetry, overlay,
									 "180-degree opposite-side imbalance -> drives Syncopation, Variation & Scale.");

					ImGui::EndTable();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::TextDisabled("No active shape song.");
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Shape Musical Parameters", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				if (music.isShapeParamsOverridden())
				{
					ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "Manual override active");
				}
				const auto p = music.getShapeParameters();
				ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.38f, 0.44f, 0.74f, 1.0f));
				if (beginInspectorTable("##ShapeMusicalParamsTable", 122.0f))
				{
					char overlay[32];

					std::snprintf(overlay, sizeof(overlay), "%.2fx", p.tempoFactor);
					drawMetricBarRow("Tempo Factor", (p.tempoFactor - 0.70f) / (1.40f - 0.70f), overlay,
									 "Driven by Motion Speed & lightness [0.72x .. 1.30x].");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.melodyDensity);
					drawMetricBarRow("Melody Density", p.melodyDensity, overlay, "Driven by Spikiness & Motion Speed.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.harmonyRichness);
					drawMetricBarRow("Harmony Richness", p.harmonyRichness, overlay,
									 "Driven by Size / Mass & Core Solidity.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.bassWeight);
					drawMetricBarRow("Bass Weight", p.bassWeight, overlay, "Driven by Size / Mass & Core Solidity.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.percEnergy);
					drawMetricBarRow("Percussion Energy", p.percEnergy, overlay, "Driven by Motion Speed & Spikiness.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.brightness);
					drawMetricBarRow("Brightness", p.brightness, overlay, "Driven by Spikiness / Sharpness.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.syncopation);
					drawMetricBarRow("Syncopation", p.syncopation, overlay, "Driven by Asymmetry & Hollowness.");

					std::snprintf(overlay, sizeof(overlay), "%.2f", p.variation);
					drawMetricBarRow("Variation", p.variation, overlay, "Driven by Spikiness, Hollowness & Asymmetry.");

					ImGui::EndTable();
				}
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::TextDisabled("No active shape song.");
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Instrument Rack (AST Driven)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const auto rack = music.getInstrumentRack();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Assigned Waveforms & Kit:");

			auto renderTrackStatusBadge = [&](WeirdAudio::MusicTrack track)
			{
				auto state = music.getTrackPlayState(track);
				const char* label = "[Playing]";
				ImVec4 color = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
				switch (state)
				{
					case WeirdAudio::TrackPlayState::Playing:
						label = "[Playing]";
						color = ImVec4(0.3f, 0.9f, 0.4f, 1.0f);
						break;
					case WeirdAudio::TrackPlayState::Paused:
						label = "[Breakdown]";
						color = ImVec4(0.9f, 0.8f, 0.3f, 1.0f);
						break;
					case WeirdAudio::TrackPlayState::Ducked:
						label = "[Ducked]";
						color = ImVec4(0.6f, 0.5f, 0.9f, 1.0f);
						break;
					case WeirdAudio::TrackPlayState::Surged:
						label = "[Surged]";
						color = ImVec4(1.0f, 0.6f, 0.1f, 1.0f);
						break;
					case WeirdAudio::TrackPlayState::Dead:
						label = "[Dead]";
						color = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
						break;
					case WeirdAudio::TrackPlayState::Muted:
					default:
						label = "[Muted]";
						color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
						break;
				}
				float badgeColWidth = ImGui::CalcTextSize("[Breakdown]").x + 8.0f;
				float startX = ImGui::GetCursorPosX();
				ImGui::TextColored(color, "%s", label);
				ImGui::SameLine(startX + badgeColWidth);
			};

			float trackNameColWidth = (std::max)({ImGui::CalcTextSize("Lead:").x, ImGui::CalcTextSize("Bass:").x,
												  ImGui::CalcTextSize("Pad:").x, ImGui::CalcTextSize("Drums:").x}) +
									  8.0f;

			auto renderTrackRow = [&](const char* checkboxId, const char* trackLabel, WeirdAudio::MusicTrack track,
									  const char* timbreName)
			{
				bool enabled = music.isTrackEnabled(track);
				if (ImGui::Checkbox(checkboxId, &enabled))
				{
					music.setTrackEnabled(track, enabled);
				}
				ImGui::SameLine();
				float startX = ImGui::GetCursorPosX();
				ImGui::TextUnformatted(trackLabel);
				ImGui::SameLine(startX + trackNameColWidth);
				renderTrackStatusBadge(track);
				ImGui::TextWrapped("%s", timbreName);
			};

			renderTrackRow("##lead_toggle", "Lead:", WeirdAudio::MusicTrack::Lead,
						   AudioPreviewManager::getWaveTypeName(rack.lead));
			renderTrackRow("##bass_toggle", "Bass:", WeirdAudio::MusicTrack::Bass,
						   AudioPreviewManager::getWaveTypeName(rack.bass));
			renderTrackRow("##pad_toggle", "Pad:", WeirdAudio::MusicTrack::Pad,
						   AudioPreviewManager::getWaveTypeName(rack.pad));
			renderTrackRow("##drums_toggle", "Drums:", WeirdAudio::MusicTrack::Drums,
						   AudioPreviewManager::getDrumKitName(rack.drumKit));

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Synthesis Parameters:");
			if (beginInspectorTable("##SynthParamsTable", 110.0f))
			{
				char buf[32];
				std::snprintf(buf, sizeof(buf), "%.2f", rack.pulseWidth);
				drawKeyValueRow("Pulse Width", buf, "Derived from AST structural hash [0.22 .. 0.48].");

				std::snprintf(buf, sizeof(buf), "%.2f", rack.fmModIndex);
				drawKeyValueRow("FM Mod Index", buf, "Derived from Spikiness & AST structural hash.");

				std::snprintf(buf, sizeof(buf), "%.2f", rack.foldDrive);
				drawKeyValueRow("Fold Drive", buf, "Derived from Spikiness & Size / Mass.");

				ImGui::EndTable();
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("AST Topology & Arrangement", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (song)
			{
				const auto& fp = song->getFingerprint();
				const uint32_t arch = fp.structuralHash % 3u;
				const char* archNames[3] = {"Groove-First (32-Bar A/B)", "Ambient-First (Floating Intro)",
											"Driving Minimalist"};

				const float playheadBeat = music.getPlayheadBeat();
				int currentStep = static_cast<int>(std::lround(playheadBeat * 4.0f)) % 512;
				if (currentStep < 0)
				{
					currentStep += 512;
				}
				const int stepInBar = currentStep % 16;
				const int barInSong = (currentStep / 16) % 32;
				const int phraseInSong = barInSong / 4;
				const char* phraseNames[8] = {"1: Intro (No Lead)",		"2: Section A1 (Main Theme)",
											  "3: Section A2 (Answer)", "4: Groove Breather",
											  "5: Section B Bridge",	"6: Breakdown (No Drums)",
											  "7: Section A3 (Return)", "8: Outro / Cool-Down"};

				if (beginInspectorTable("##AstArrangementTable", 115.0f))
				{
					char buf[96];

					std::snprintf(buf, sizeof(buf), "0x%08X", fp.structuralHash);
					drawKeyValueRow("Structural Seed", buf,
									"Topological FNV-1a hash (ignores constants & var0..var7).");

					std::snprintf(buf, sizeof(buf), "%d", fp.nodeCount);
					drawKeyValueRow("Total Operators", buf);

					std::snprintf(buf, sizeof(buf), "%d", fp.maxDepth);
					drawKeyValueRow("Tree Depth", buf);

					std::snprintf(buf, sizeof(buf), "%d", fp.branchCount);
					drawKeyValueRow("Branching Nodes", buf);

					drawKeyValueRow("Arrangement", archNames[arch], "Derived from Structural Seed mod 3.");
					drawKeyValueRow("Section", phraseNames[phraseInSong]);

					std::snprintf(buf, sizeof(buf), "Bar %d/32 | Step %d/16 | Loop #%d", barInSong + 1, stepInBar + 1,
								  music.getLoopCycle() + 1);
					drawKeyValueRow("Playhead", buf);

					ImGui::EndTable();
				}
			}
			else
			{
				ImGui::TextDisabled("No active shape song.");
			}
		}

		ImGui::Spacing();
		if (ImGui::CollapsingHeader("Live Oscilloscope", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const auto& audioData = WeirdAudio::AudioEngine::getInstance().getAudioData();
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
		if (ImGui::CollapsingHeader("Dynamic Feedback Triggers", ImGuiTreeNodeFlags_DefaultOpen))
		{
			float surgeLvl = music.getSurgeLevel();
			ImGui::ProgressBar(surgeLvl, ImVec2(-1.0f, 0.0f), "Surge Hype (Combo)");
			float duckLvl = music.getDuckingLevel();
			ImGui::ProgressBar(duckLvl, ImVec2(-1.0f, 0.0f), "Duck Danger (Rough/Low)");

			float availWidth = ImGui::GetContentRegionAvail().x;
			float gap = ImGui::GetStyle().ItemSpacing.x;
			float btnWidth1 = std::floor((availWidth - gap) * 0.5f);
			float btnWidth2 = availWidth - btnWidth1 - gap;
			float btnHeight = 25.0f;

			if (ImGui::Button("Surge (+Combo)", ImVec2(btnWidth1, btnHeight)))
				music.surge(0.40f);
			ImGui::SameLine();
			if (ImGui::Button("Duck (Danger)", ImVec2(btnWidth2, btnHeight)))
				music.duck(0.50f);

			if (ImGui::Button("Positive (Click)", ImVec2(btnWidth1, btnHeight)))
				music.triggerPositiveFeedback(1.0f);
			ImGui::SameLine();
			if (ImGui::Button("Negative (Error)", ImVec2(btnWidth2, btnHeight)))
				music.triggerNegativeFeedback(1.0f);

			if (ImGui::Button("Death Trigger", ImVec2(btnWidth1, btnHeight)))
				music.triggerDeath();

			ImGui::Spacing();
			if (ImGui::Button("Reset All Dynamic Effects", ImVec2(availWidth, btnHeight)))
				music.resetDynamicEffects();
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
} // namespace WeirdEngine::Tools::ShapeEd
