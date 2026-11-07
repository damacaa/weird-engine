#include "InstrumentScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <imgui.h>

#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;
using namespace WeirdEngine::WeirdAudio;

namespace InstrumentSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "InstrumentScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		auto& state = registry.emplaceState<State>();

		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		// Ensure music engine is active and audible
		services.audio().music().setPlaying(true);
		if (services.audio().music().getVolume() < 0.15f)
		{
			services.audio().music().setVolume(0.80f);
		}

		// Create 2D materials for piano keys only
		auto& whiteMat = services.materials2D().createMaterial("inst_key_white");
		whiteMat.color = vec4(0.92f, 0.94f, 0.97f, 1.0f);
		state.whiteKeyMat = whiteMat.id;

		auto& blackMat = services.materials2D().createMaterial("inst_key_black");
		blackMat.color = vec4(0.12f, 0.14f, 0.18f, 1.0f);
		state.blackKeyMat = blackMat.id;

		auto& whiteActive = services.materials2D().createMaterial("inst_key_white_active");
		whiteActive.color = vec4(0.35f, 0.85f, 1.0f, 1.0f);
		state.whiteActiveMat = whiteActive.id;

		auto& blackActive = services.materials2D().createMaterial("inst_key_black_active");
		blackActive.color = vec4(0.15f, 0.55f, 0.95f, 1.0f);
		state.blackActiveMat = blackActive.id;
	}

	void setupSceneSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		state.lastWidth = Display::width;
		state.lastHeight = Display::height;

		// Visual Piano Keyboard (2 Octaves: 14 White Keys + 10 Black Keys)
		// White key offsets in semitones: C(0), D(2), E(4), F(5), G(7), A(9), B(11), C(12), D(14), E(16), F(17), G(19), A(21), B(23)
		static const int whiteOffsets[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23};
		static const size_t numWhite = sizeof(whiteOffsets) / sizeof(whiteOffsets[0]);

		// Black key offsets and the white key index they sit between
		struct BlackDef
		{
			int semitone;
			int afterWhiteIdx;
		};
		static const BlackDef blackDefs[] = {
			{1, 0},	  // C#
			{3, 1},	  // D#
			{6, 3},	  // F#
			{8, 4},	  // G#
			{10, 5},  // A#
			{13, 7},  // C#+1
			{15, 8},  // D#+1
			{18, 10}, // F#+1
			{20, 11}, // G#+1
			{22, 12}  // A#+1
		};
		static const size_t numBlack = sizeof(blackDefs) / sizeof(blackDefs[0]);

		float keyboardW = (std::min)(winW - 60.0f, 740.0f);
		float whiteW = keyboardW / static_cast<float>(numWhite);
		float whiteH = 200.0f;
		float blackW = whiteW * 0.60f;
		float blackH = whiteH * 0.62f;
		float startX = (winW - keyboardW) * 0.5f;
		float startY = (winH - whiteH) * 0.5f; // Centered vertically

		state.keys.clear();
		state.keys.reserve(numWhite + numBlack);

		// First create white keys
		for (size_t i = 0; i < numWhite; ++i)
		{
			float kx = startX + static_cast<float>(i) * whiteW + whiteW * 0.5f;
			float ky = startY + whiteH * 0.5f;

			auto entity = services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
														.variables = {{DefaultShapes::Box::PosX, kx},
																	  {DefaultShapes::Box::PosY, ky},
																	  {DefaultShapes::Box::SizeX, (whiteW - 3.0f) * 0.5f},
																	  {DefaultShapes::Box::SizeY, (whiteH - 3.0f) * 0.5f}},
														.material = services.materials2D().get("inst_key_white")});
			auto& btn = registry.addComponent<ShapeButton>(entity);
			btn.clickPadding = 0.0f;

			KeyVisual kv;
			kv.midiNote = 48 + whiteOffsets[i]; // Octave 3 base + offset
			kv.isBlack = false;
			kv.entity = entity;
			kv.basePosX = kx;
			kv.basePosY = ky;
			kv.sizeX = (whiteW - 3.0f) * 0.5f;
			kv.sizeY = (whiteH - 3.0f) * 0.5f;
			state.keys.push_back(kv);
		}

		// Then create black keys (rendered on top of white keys)
		for (size_t i = 0; i < numBlack; ++i)
		{
			const auto& bd = blackDefs[i];
			float kx = startX + static_cast<float>(bd.afterWhiteIdx + 1) * whiteW;
			float ky = startY + blackH * 0.5f;

			auto entity = services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
														.variables = {{DefaultShapes::Box::PosX, kx},
																	  {DefaultShapes::Box::PosY, ky},
																	  {DefaultShapes::Box::SizeX, blackW * 0.5f},
																	  {DefaultShapes::Box::SizeY, blackH * 0.5f}},
														.material = services.materials2D().get("inst_key_black")});
			auto& btn = registry.addComponent<ShapeButton>(entity);
			btn.clickPadding = 0.0f;

			KeyVisual kv;
			kv.midiNote = 48 + bd.semitone;
			kv.isBlack = true;
			kv.entity = entity;
			kv.basePosX = kx;
			kv.basePosY = ky;
			kv.sizeX = blackW * 0.5f;
			kv.sizeY = blackH * 0.5f;
			state.keys.push_back(kv);
		}
	}

	void inputHandlingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		auto& input = services.input();

		// 1. Instrument Selection with number keys 1-6 or TAB
		if (input.getKeyDown(Input::Num1))
			state.selectedInstrument = Instrument::Lead;
		if (input.getKeyDown(Input::Num2))
			state.selectedInstrument = Instrument::Bass;
		if (input.getKeyDown(Input::Num3))
			state.selectedInstrument = Instrument::Pad;
		if (input.getKeyDown(Input::Num4))
			state.selectedInstrument = Instrument::Kick;
		if (input.getKeyDown(Input::Num5))
			state.selectedInstrument = Instrument::Snare;
		if (input.getKeyDown(Input::Num6))
			state.selectedInstrument = Instrument::HiHat;

		if (input.getKeyDown(Input::Tab))
		{
			int cur = 0;
			for (size_t i = 0; i < SongInstruments.size(); ++i)
			{
				if (SongInstruments[i] == state.selectedInstrument)
				{
					cur = static_cast<int>(i);
					break;
				}
			}
			state.selectedInstrument = SongInstruments[(cur + 1) % SongInstruments.size()];
		}

		// 2. Octave Shift with Z / X or Up / Down
		if (input.getKeyDown(Input::Z) || input.getKeyDown(Input::Down))
		{
			state.currentOctave = (std::max)(1, state.currentOctave - 1);
		}
		if (input.getKeyDown(Input::X) || input.getKeyDown(Input::Up))
		{
			state.currentOctave = (std::min)(7, state.currentOctave + 1);
		}

		auto playNoteMidi = [&](int midiNote, Instrument inst)
		{
			if (midiNote < 0 || midiNote > 127)
				return;
			services.audio().playNote(midiNote, state.noteAmp, state.noteDuration, inst, state.pan,
									  state.filterCutoff);
			state.keyFlashTimers[midiNote] = 0.25f;
			state.lastPlayedMidi = midiNote;
			state.lastPlayedNoteName = SdfSong::midiToNoteString(midiNote);
			state.lastPlayedFreq = SdfSong::midiToFrequency(midiNote);
		};

		int baseMidi = state.currentOctave * 12; // C of current octave

		// 3. Musical Keyboard Key Bindings (DAW Tracker Layout)
		// White keys
		if (input.getKeyDown(Input::A))
			playNoteMidi(baseMidi + 0, state.selectedInstrument); // C
		if (input.getKeyDown(Input::S))
			playNoteMidi(baseMidi + 2, state.selectedInstrument); // D
		if (input.getKeyDown(Input::D))
			playNoteMidi(baseMidi + 4, state.selectedInstrument); // E
		if (input.getKeyDown(Input::F))
			playNoteMidi(baseMidi + 5, state.selectedInstrument); // F
		if (input.getKeyDown(Input::G))
			playNoteMidi(baseMidi + 7, state.selectedInstrument); // G
		if (input.getKeyDown(Input::H))
			playNoteMidi(baseMidi + 9, state.selectedInstrument); // A
		if (input.getKeyDown(Input::J))
			playNoteMidi(baseMidi + 11, state.selectedInstrument); // B
		if (input.getKeyDown(Input::K))
			playNoteMidi(baseMidi + 12, state.selectedInstrument); // C (+1)
		if (input.getKeyDown(Input::L))
			playNoteMidi(baseMidi + 14, state.selectedInstrument); // D (+1)
		if (input.getKeyDown(Input::Semicolon))
			playNoteMidi(baseMidi + 16, state.selectedInstrument); // E (+1)

		// Black keys (Sharps)
		if (input.getKeyDown(Input::W))
			playNoteMidi(baseMidi + 1, state.selectedInstrument); // C#
		if (input.getKeyDown(Input::E))
			playNoteMidi(baseMidi + 3, state.selectedInstrument); // D#
		if (input.getKeyDown(Input::T))
			playNoteMidi(baseMidi + 6, state.selectedInstrument); // F#
		if (input.getKeyDown(Input::Y))
			playNoteMidi(baseMidi + 8, state.selectedInstrument); // G#
		if (input.getKeyDown(Input::U))
			playNoteMidi(baseMidi + 10, state.selectedInstrument); // A#
		if (input.getKeyDown(Input::O))
			playNoteMidi(baseMidi + 13, state.selectedInstrument); // C# (+1)
		if (input.getKeyDown(Input::P))
			playNoteMidi(baseMidi + 15, state.selectedInstrument); // D# (+1)

		// 4. Quick Percussion Key Bindings
		if (input.getKeyDown(Input::B))
			playNoteMidi(36, Instrument::Kick); // Low kick
		if (input.getKeyDown(Input::N))
			playNoteMidi(38, Instrument::Snare); // Snare
		if (input.getKeyDown(Input::M))
			playNoteMidi(42, Instrument::HiHat); // Hi-Hat
		if (input.getKeyDown(Input::Space))
			playNoteMidi(baseMidi + 0, state.selectedInstrument);

		// 5. Mouse Clicks on On-Screen Piano Keys
		for (auto& k : state.keys)
		{
			if (!registry.isEntityValid(k.entity) || !registry.hasComponent<ShapeButton>(k.entity))
				continue;

			auto& btn = registry.getComponent<ShapeButton>(k.entity);
			if (btn.state == ButtonState::Down)
			{
				int playedMidi = k.midiNote + (state.currentOctave - 4) * 12;
				playNoteMidi(playedMidi, state.selectedInstrument);
			}
		}
	}

	void updateVisualsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float dt = services.time().deltaTime();

		// Decrement key flash timers
		for (size_t i = 0; i < 128; ++i)
		{
			if (state.keyFlashTimers[i] > 0.0f)
			{
				state.keyFlashTimers[i] = (std::max)(0.0f, state.keyFlashTimers[i] - dt);
			}
		}

		// Update key materials based on active flashes
		for (auto& k : state.keys)
		{
			if (!registry.isEntityValid(k.entity) || !registry.hasComponent<UIShape>(k.entity))
				continue;

			int actualMidi = k.midiNote + (state.currentOctave - 4) * 12;
			bool isFlashing = (actualMidi >= 0 && actualMidi < 128 && state.keyFlashTimers[actualMidi] > 0.0f);

			auto& shape = registry.getComponent<UIShape>(k.entity);
			Material2DHandle targetMat;
			if (k.isBlack)
			{
				targetMat = isFlashing ? state.blackActiveMat : state.blackKeyMat;
			}
			else
			{
				targetMat = isFlashing ? state.whiteActiveMat : state.whiteKeyMat;
			}

			if (shape.material != targetMat.id)
			{
				shape.material = targetMat.id;
				registry.setComponentDirty(shape);
			}
		}
	}
} // namespace InstrumentSceneNamespace

InstrumentScene::InstrumentScene()
{
	addStartSystem(InstrumentSceneNamespace::stateInitSystem);
	addStartSystem(InstrumentSceneNamespace::setupSceneSystem);

	addUpdateSystem(GlobalSystems::sceneControlSystem);
	addUpdateSystem(InstrumentSceneNamespace::inputHandlingSystem);
	addUpdateSystem(InstrumentSceneNamespace::updateVisualsSystem);
}

void InstrumentScene::onImGuiRender(Registry& registry, ServiceProvider& services)
{
#ifndef WEIRD_DISABLE_IMGUI
	auto* pState = registry.getState<InstrumentSceneNamespace::State>();
	if (!pState)
		return;
	auto& state = *pState;
	auto& musicEngine = services.audio().music();
	auto rack = musicEngine.getInstrumentRack();
	bool rackChanged = false;

	// Render directly inside the settings tab (F3 / Engine Settings)
	if (ImGui::CollapsingHeader("Instrument Studio", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		// 1. Active Instrument Picker & Status
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Active Instrument & Performance:");

		const char* instNames[] = {"Lead", "Bass", "Pad", "Kick", "Snare", "Hi-Hat"};
		int currentInstIdx = 0;
		for (size_t i = 0; i < SongInstruments.size(); ++i)
		{
			if (SongInstruments[i] == state.selectedInstrument)
			{
				currentInstIdx = static_cast<int>(i);
				break;
			}
		}

		if (ImGui::Combo("Instrument", &currentInstIdx, instNames, IM_ARRAYSIZE(instNames)))
		{
			state.selectedInstrument = SongInstruments[currentInstIdx];
		}

		// Current Timbre Summary
		const char* subType = "";
		switch (state.selectedInstrument)
		{
			case Instrument::Lead:
				subType = SdfMusicEngine::getWaveTypeName(rack.lead);
				break;
			case Instrument::Bass:
				subType = SdfMusicEngine::getWaveTypeName(rack.bass);
				break;
			case Instrument::Pad:
				subType = SdfMusicEngine::getWaveTypeName(rack.pad);
				break;
			case Instrument::Kick:
			case Instrument::Snare:
			case Instrument::HiHat:
				subType = SdfMusicEngine::getDrumKitName(rack.drumKit);
				break;
			default:
				break;
		}
		ImGui::Text("Timbre: %s", subType);

		// Octave Selector
		ImGui::SliderInt("Octave", &state.currentOctave, 1, 7);

		// Last Note Display
		if (state.lastPlayedMidi >= 0)
		{
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Last Played Note: %s (MIDI %d, %.1f Hz)",
							   state.lastPlayedNoteName.c_str(), state.lastPlayedMidi, state.lastPlayedFreq);
		}
		else
		{
			ImGui::TextDisabled("Last Played Note: None (Play a note on piano or keyboard)");
		}

		// 2. Sound & Timbre Parameters
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Instrument Sound & Timbre Parameters:");

		const char* waveNames[] = {"Soft Sine", "Bandlimited Saw", "Pulse / Square", "FM Pluck", "Wavefolder"};
		if (state.selectedInstrument == Instrument::Lead)
		{
			int waveIdx = static_cast<int>(rack.lead);
			if (ImGui::Combo("Lead Waveform", &waveIdx, waveNames, 5))
			{
				rack.lead = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
		}
		else if (state.selectedInstrument == Instrument::Bass)
		{
			int waveIdx = static_cast<int>(rack.bass);
			if (ImGui::Combo("Bass Waveform", &waveIdx, waveNames, 5))
			{
				rack.bass = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
		}
		else if (state.selectedInstrument == Instrument::Pad)
		{
			int waveIdx = static_cast<int>(rack.pad);
			if (ImGui::Combo("Pad Waveform", &waveIdx, waveNames, 5))
			{
				rack.pad = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
		}
		else // Drums: Kick, Snare, Hi-Hat
		{
			const char* kitNames[] = {"808 Style (Deep & Subby)", "Punchy Acoustic (Organic Wood)",
									  "Industrial (Overdriven Metal)"};
			int kitIdx = rack.drumKit;
			if (ImGui::Combo("Drum Kit Timbre", &kitIdx, kitNames, 3))
			{
				rack.drumKit = kitIdx;
				rackChanged = true;
			}
		}

		// Synthesis Modulation Sliders
		ImGui::Spacing();
		ImGui::TextDisabled("Synthesis Modulation:");
		if (ImGui::SliderFloat("Pulse Width", &rack.pulseWidth, 0.05f, 0.50f, "%.2f"))
		{
			rackChanged = true;
		}
		if (ImGui::SliderFloat("FM Mod Index", &rack.fmModIndex, 0.0f, 8.0f, "%.2f"))
		{
			rackChanged = true;
		}
		if (ImGui::SliderFloat("Wavefold Drive", &rack.foldDrive, 0.0f, 6.0f, "%.2f"))
		{
			rackChanged = true;
		}

		if (rackChanged)
		{
			musicEngine.setInstrumentRack(rack);
		}

		// 3. Voice Shaping (Filter & Envelope)
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Voice Shaping (Filter & Envelope):");
		ImGui::SliderFloat("Filter Cutoff", &state.filterCutoff, 100.0f, 20000.0f, "%.0f Hz",
						   ImGuiSliderFlags_Logarithmic);
		ImGui::SliderFloat("Filter Resonance (Q)", &state.filterQ, 0.5f, 4.0f, "%.2f");
		ImGui::SliderFloat("Note Duration / Decay", &state.noteDuration, 0.05f, 3.0f, "%.2f s");
		ImGui::SliderFloat("Note Velocity / Amp", &state.noteAmp, 0.1f, 1.5f, "%.2f");
		ImGui::SliderFloat("Stereo Pan", &state.pan, -1.0f, 1.0f, "%.2f");

		// 4. Audition & Trigger Buttons
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Note Audition Buttons:");

		auto playMidi = [&](int midi)
		{
			services.audio().playNote(midi, state.noteAmp, state.noteDuration, state.selectedInstrument, state.pan,
									  state.filterCutoff);
			state.keyFlashTimers[midi] = 0.25f;
			state.lastPlayedMidi = midi;
			state.lastPlayedNoteName = SdfSong::midiToNoteString(midi);
			state.lastPlayedFreq = SdfSong::midiToFrequency(midi);
		};

		static const char* noteLabels[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		int baseMidi = state.currentOctave * 12;
		for (int i = 0; i < 12; ++i)
		{
			if (i > 0 && i % 4 != 0)
			{
				ImGui::SameLine();
			}
			char btnLabel[32];
			std::snprintf(btnLabel, sizeof(btnLabel), "%s%d##imgui_note_%d", noteLabels[i], state.currentOctave, i);
			if (ImGui::Button(btnLabel, ImVec2(52, 28)))
			{
				playMidi(baseMidi + i);
			}
		}

		ImGui::Spacing();
		if (ImGui::Button("Major Triad"))
		{
			playMidi(baseMidi + 0);
			playMidi(baseMidi + 4);
			playMidi(baseMidi + 7);
		}
		ImGui::SameLine();
		if (ImGui::Button("Minor Triad"))
		{
			playMidi(baseMidi + 0);
			playMidi(baseMidi + 3);
			playMidi(baseMidi + 7);
		}
		ImGui::SameLine();
		if (ImGui::Button("7th Chord"))
		{
			playMidi(baseMidi + 0);
			playMidi(baseMidi + 4);
			playMidi(baseMidi + 7);
			playMidi(baseMidi + 11);
		}

		// 5. Controls Reference Guide
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::TreeNode("Keyboard Shortcuts Reference"))
		{
			ImGui::BulletText("1 - 6: Pick Instrument (Lead, Bass, Pad, Kick, Snare, Hi-Hat) | TAB: Cycle");
			ImGui::BulletText("Z / X or Down / Up: Octave Shift (- / +)");
			ImGui::BulletText("White Keys: A (C), S (D), D (E), F (F), G (G), H (A), J (B), K (C+1), L (D+1), ; (E+1)");
			ImGui::BulletText("Black Keys: W (C#), E (D#), T (F#), Y (G#), U (A#), O (C#+1), P (D#+1)");
			ImGui::BulletText("Drums: B (Kick), N (Snare), M (Hi-Hat) | Space: Play Root Note");
			ImGui::BulletText("ESC: Return to Main Menu");
			ImGui::TreePop();
		}

		ImGui::Unindent();
	}
#endif
}
