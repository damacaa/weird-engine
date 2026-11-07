#include "InstrumentScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

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

		// Sync arpeggiator BPM (and scale/root) to active song if one is playing
		if (auto activeSong = services.audio().music().getCurrentSong())
		{
			state.arpBpm = services.audio().music().getTempo();
			state.highlightScaleIdx = static_cast<int>(activeSong->getScale());
			state.highlightRootNote = ((activeSong->getRootMidi() % 12) + 12) % 12;
		}

		// Create 2D materials for piano keys only
		auto& whiteMat = services.materials2D().createMaterial("inst_key_white");
		whiteMat.color = vec4(0.92f, 0.94f, 0.97f, 1.0f);
		state.whiteKeyMat = whiteMat.id;

		auto& blackMat = services.materials2D().createMaterial("inst_key_black");
		blackMat.color = vec4(0.12f, 0.14f, 0.18f, 1.0f);
		state.blackKeyMat = blackMat.id;

		auto& whiteScale = services.materials2D().createMaterial("inst_key_white_scale");
		whiteScale.color = vec4(0.84f, 0.96f, 0.86f, 1.0f);
		state.whiteScaleMat = whiteScale.id;

		auto& blackScale = services.materials2D().createMaterial("inst_key_black_scale");
		blackScale.color = vec4(0.18f, 0.36f, 0.24f, 1.0f);
		state.blackScaleMat = blackScale.id;
	}

	static bool isNoteInSelectedScale(int midiNote, int scaleIdx, int rootNote)
	{
		if (scaleIdx < 0)
			return false; // Scale highlight Off
		int rel = ((midiNote - rootNote) % 12 + 12) % 12;
		auto check = [&](const int* arr, int sz)
		{
			for (int i = 0; i < sz; ++i)
			{
				if (arr[i] == rel)
					return true;
			}
			return false;
		};
		static const int pentMajor[] = {0, 2, 4, 7, 9};
		static const int pentMinor[] = {0, 3, 5, 7, 10};
		static const int major[] = {0, 2, 4, 5, 7, 9, 11};
		static const int minor[] = {0, 2, 3, 5, 7, 8, 10};
		static const int dorian[] = {0, 2, 3, 5, 7, 9, 10};
		static const int lydian[] = {0, 2, 4, 6, 7, 9, 11};
		switch (scaleIdx)
		{
			case 0:
				return check(pentMajor, 5);
			case 1:
				return check(pentMinor, 5);
			case 2:
				return check(major, 7);
			case 3:
				return check(minor, 7);
			case 4:
				return check(dorian, 7);
			case 5:
				return check(lydian, 7);
			case 6:
				return true; // Chromatic
			default:
				return false;
		}
	}

	static const char* getSolfeggioName(int midiNote)
	{
		static const char* solf[] = {"Do", "Do#", "Re", "Re#", "Mi", "Fa", "Fa#", "Sol", "Sol#", "La", "La#", "Si"};
		if (midiNote < 0 || midiNote > 127)
			return "--";
		return solf[midiNote % 12];
	}

	void setupSceneSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		state.lastWidth = Display::width;
		state.lastHeight = Display::height;

		// Visual Piano Keyboard (2 Octaves: 14 White Keys + 10 Black Keys)
		// White key offsets in semitones: C(0), D(2), E(4), F(5), G(7), A(9), B(11), C(12), D(14), E(16), F(17), G(19),
		// A(21), B(23)
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

			auto entity =
				services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
											  .variables = {{DefaultShapes::Box::PosX, kx},
															{DefaultShapes::Box::PosY, ky},
															{DefaultShapes::Box::SizeX, (whiteW - 3.0f) * 0.5f},
															{DefaultShapes::Box::SizeY, (whiteH - 3.0f) * 0.5f}},
											  .material = services.materials2D().get("inst_key_white")});
			auto& btn = registry.addComponent<ShapeButton>(entity);
			btn.clickPadding = 0.0f;

			KeyVisual kv;
			kv.midiNote = 60 + whiteOffsets[i]; // Octave 4 base (Middle C = 60) + offset
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
			kv.midiNote = 60 + bd.semitone;
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

		// Read gamepad axes
		float stickX = input.getGamepadAxis(Input::GamepadAxis::LeftX);
		float stickY = input.getGamepadAxis(Input::GamepadAxis::LeftY);
		float rStickX = input.getGamepadAxis(Input::GamepadAxis::RightX);
		float rStickY = input.getGamepadAxis(Input::GamepadAxis::RightY);
		float leftTrigger = input.getGamepadAxis(Input::GamepadAxis::LeftTrigger);
		float rightTrigger = input.getGamepadAxis(Input::GamepadAxis::RightTrigger);

		bool stickLeftDown = (stickX < -0.5f && state.prevStickX >= -0.5f);
		bool stickRightDown = (stickX > 0.5f && state.prevStickX <= 0.5f);
		bool stickUpDown = (stickY < -0.5f && state.prevStickY >= -0.5f);
		bool stickDownDown = (stickY > 0.5f && state.prevStickY <= 0.5f);

		bool rStickLeftDown = (rStickX < -0.5f && state.prevRightStickX >= -0.5f);
		bool rStickRightDown = (rStickX > 0.5f && state.prevRightStickX <= 0.5f);
		bool rStickUpDown = (rStickY < -0.5f && state.prevRightStickY >= -0.5f);
		bool rStickDownDown = (rStickY > 0.5f && state.prevRightStickY <= 0.5f);

		bool leftTriggerDown = (leftTrigger > 0.5f && state.prevLeftTrigger <= 0.5f);
		bool rightTriggerDown = (rightTrigger > 0.5f && state.prevRightTrigger <= 0.5f);

		state.prevStickX = stickX;
		state.prevStickY = stickY;
		state.prevRightStickX = rStickX;
		state.prevRightStickY = rStickY;
		state.prevLeftTrigger = leftTrigger;
		state.prevRightTrigger = rightTrigger;

		// 1. Instrument Selection with number keys 1-6, TAB, Start button, or Right Stick Left/Right
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

		if (input.getKeyDown(Input::Tab) || input.getGamepadButtonDown(Input::GamepadButton::Start) || rStickRightDown)
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
		else if (rStickLeftDown)
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
			state.selectedInstrument = SongInstruments[(cur - 1 + SongInstruments.size()) % SongInstruments.size()];
		}

		// 2. Octave Shift with Z / X, Up / Down, Triggers, or Right Stick Up/Down
		if (input.getKeyDown(Input::Z) || input.getKeyDown(Input::Down) || leftTriggerDown || rStickDownDown)
		{
			state.currentOctave = (std::max)(1, state.currentOctave - 1);
		}
		if (input.getKeyDown(Input::X) || input.getKeyDown(Input::Up) || rightTriggerDown || rStickUpDown)
		{
			state.currentOctave = (std::min)(7, state.currentOctave + 1);
		}

		auto playNoteMidi = [&](int midiNote, Instrument inst)
		{
			if (midiNote < 0 || midiNote > 127)
				return;
			services.audio().playNote(midiNote, state.noteAmp, state.noteDuration, inst, state.pan, state.filterCutoff,
									  state.filterQ);
			state.keyFlashTimers[midiNote] = 0.25f;
			state.lastPlayedMidi = midiNote;
			state.lastPlayedNoteName = SdfSong::midiToNoteString(midiNote);
			state.lastPlayedFreq = SdfSong::midiToFrequency(midiNote);
		};

		int baseMidi = (state.currentOctave + 1) * 12; // C of current octave (MIDI 60 = C4 when currentOctave = 4)

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

		// 4. Gamepad Musical Performance
		// Modifiers: Left Shoulder = Black Keys (Accidentals), Right Shoulder = Upper Octave
		bool blackKeyMod = input.getGamepadButton(Input::GamepadButton::LeftShoulder);
		bool octaveUpMod = input.getGamepadButton(Input::GamepadButton::RightShoulder);
		int octaveOffset = octaveUpMod ? 12 : 0;

		// D-Pad / Left Stick: Notes 1 to 4 (Do, Re, Mi, Fa)
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadLeft) || stickLeftDown)
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 1 : 0), state.selectedInstrument); // C or C#
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadDown) || stickDownDown)
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 3 : 2), state.selectedInstrument); // D or D#
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadRight) || stickRightDown)
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 3 : 4), state.selectedInstrument); // E or Eb (D#)
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadUp) || stickUpDown)
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 6 : 5), state.selectedInstrument); // F or F#

		// Face Buttons: Notes 5 to 8 (Sol, La, Si, High Do)
		if (input.getGamepadButtonDown(Input::GamepadButton::West))
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 8 : 7), state.selectedInstrument); // G or G#
		if (input.getGamepadButtonDown(Input::GamepadButton::South))
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 10 : 9), state.selectedInstrument); // A or A#
		if (input.getGamepadButtonDown(Input::GamepadButton::East))
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 10 : 11), state.selectedInstrument); // B or Bb (A#)
		if (input.getGamepadButtonDown(Input::GamepadButton::North))
			playNoteMidi(baseMidi + octaveOffset + (blackKeyMod ? 13 : 12), state.selectedInstrument); // High C or C#

		// Quick Percussion via Stick Clicks
		if (input.getGamepadButtonDown(Input::GamepadButton::LeftStick))
			playNoteMidi(36, Instrument::Kick);
		if (input.getGamepadButtonDown(Input::GamepadButton::RightStick))
			playNoteMidi(38, Instrument::Snare);

		// 5. Quick Percussion Key Bindings
		if (input.getKeyDown(Input::B))
			playNoteMidi(36, Instrument::Kick); // Low kick
		if (input.getKeyDown(Input::N))
			playNoteMidi(38, Instrument::Snare); // Snare
		if (input.getKeyDown(Input::M))
			playNoteMidi(42, Instrument::HiHat); // Hi-Hat
		if (input.getKeyDown(Input::Space))
			playNoteMidi(baseMidi + 0, state.selectedInstrument);

		// 6. Mouse Clicks on On-Screen Piano Keys
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

	void sequencerSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float dt = services.time().deltaTime();

		auto triggerMidi = [&](int midiNote, Instrument inst)
		{
			if (midiNote < 0 || midiNote > 127)
				return;
			services.audio().playNote(midiNote, state.noteAmp, state.noteDuration, inst, state.pan, state.filterCutoff,
									  state.filterQ);
			state.keyFlashTimers[midiNote] = 0.25f;
			state.lastPlayedMidi = midiNote;
			state.lastPlayedNoteName = SdfSong::midiToNoteString(midiNote);
			state.lastPlayedFreq = SdfSong::midiToFrequency(midiNote);
		};

		// 1. Process scheduled notes (strummed chords)
		for (auto it = state.scheduledNotes.begin(); it != state.scheduledNotes.end();)
		{
			it->delay -= dt;
			if (it->delay <= 0.0f)
			{
				triggerMidi(it->midiNote, it->instrument);
				it = state.scheduledNotes.erase(it);
			}
			else
			{
				++it;
			}
		}

		// 2. Process Arpeggiator / Scale Walker
		if (state.arpEnabled)
		{
			float stepInterval = (60.0f / (std::max)(30.0f, state.arpBpm)) * 0.5f; // 8th notes
			state.arpTimer += dt;
			if (state.arpTimer >= stepInterval)
			{
				state.arpTimer -= stepInterval;

				int baseMidi = (state.currentOctave + 1) * 12 + state.highlightRootNote;
				if (state.arpPattern == 3)
				{
					// Pop Chord Progression (I - V - vi - IV) arpeggio
					static const int progOffsets[4][3] = {
						{0, 4, 7},	 // I (Do-Mi-Sol)
						{7, 11, 14}, // V (Sol-Si-Re)
						{9, 12, 16}, // vi (La-Do-Mi)
						{5, 9, 12}	 // IV (Fa-La-Do)
					};
					int chordIdx = (state.arpStep / 4) % 4;
					int noteInChord = state.arpStep % 4;
					int offset = (noteInChord == 3) ? progOffsets[chordIdx][1] : progOffsets[chordIdx][noteInChord];
					triggerMidi(baseMidi + offset, state.selectedInstrument);
					state.arpStep = (state.arpStep + 1) % 16;
				}
				else
				{
					// Collect scale notes across 1 octave + top root
					std::vector<int> scaleNotes;
					int activeScale = (state.highlightScaleIdx >= 0) ? state.highlightScaleIdx : 2;
					for (int semitone = 0; semitone <= 12; ++semitone)
					{
						if (isNoteInSelectedScale(state.highlightRootNote + semitone, activeScale,
												  state.highlightRootNote))
						{
							scaleNotes.push_back(baseMidi + semitone);
						}
					}
					if (!scaleNotes.empty())
					{
						int n = static_cast<int>(scaleNotes.size());
						int idx = 0;
						if (state.arpPattern == 0) // Up
						{
							idx = state.arpStep % n;
						}
						else if (state.arpPattern == 1) // Down
						{
							idx = (n - 1) - (state.arpStep % n);
						}
						else // Up-Down
						{
							int cycle = (std::max)(1, 2 * n - 2);
							int pos = state.arpStep % cycle;
							idx = (pos < n) ? pos : (cycle - pos);
						}
						triggerMidi(scaleNotes[idx], state.selectedInstrument);
						state.arpStep++;
					}
				}
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

		// Update key press displacement & scale highlighting
		bool materialChanged = false;
		for (auto& k : state.keys)
		{
			if (!registry.isEntityValid(k.entity) || !registry.hasComponent<UIShape>(k.entity))
				continue;

			int actualMidi = k.midiNote + (state.currentOctave - 4) * 12;
			float flashTimer = (actualMidi >= 0 && actualMidi < 128) ? state.keyFlashTimers[actualMidi] : 0.0f;
			float pressRatio = std::clamp(flashTimer / 0.25f, 0.0f, 1.0f);

			auto& shape = registry.getComponent<UIShape>(k.entity);

			// Physical press effect via position & size parameters (no shader recompile needed!)
			float targetPosY = k.basePosY + (k.isBlack ? 5.0f : 7.0f) * pressRatio;
			float targetSizeX = k.sizeX * (1.0f - 0.07f * pressRatio);
			float targetSizeY = k.sizeY * (1.0f - 0.06f * pressRatio);

			if (std::abs(shape.parameters[DefaultShapes::Box::PosY] - targetPosY) > 0.01f ||
				std::abs(shape.parameters[DefaultShapes::Box::SizeX] - targetSizeX) > 0.01f ||
				std::abs(shape.parameters[DefaultShapes::Box::SizeY] - targetSizeY) > 0.01f)
			{
				shape.parameters[DefaultShapes::Box::PosY] = targetPosY;
				shape.parameters[DefaultShapes::Box::SizeX] = targetSizeX;
				shape.parameters[DefaultShapes::Box::SizeY] = targetSizeY;
				registry.setComponentDirty(shape);
			}

			// Material only changes when the user changes the scale/root/octave in the UI
			bool inScale = isNoteInSelectedScale(actualMidi, state.highlightScaleIdx, state.highlightRootNote);
			Material2DHandle targetMat = k.isBlack ? (inScale ? state.blackScaleMat : state.blackKeyMat)
												   : (inScale ? state.whiteScaleMat : state.whiteKeyMat);

			if (shape.material != targetMat.id)
			{
				shape.material = targetMat.id;
				registry.setComponentDirty(shape);
				materialChanged = true;
			}
		}

		if (materialChanged)
		{
			services.render().forceShaderRefreshUI();
		}
	}
} // namespace InstrumentSceneNamespace

InstrumentScene::InstrumentScene()
{
	addStartSystem(InstrumentSceneNamespace::stateInitSystem);
	addStartSystem(InstrumentSceneNamespace::setupSceneSystem);

	addUpdateSystem(GlobalSystems::sceneControlSystem);
	addUpdateSystem(InstrumentSceneNamespace::inputHandlingSystem);
	addUpdateSystem(InstrumentSceneNamespace::sequencerSystem);
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
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Last Played Note: %s (%s) | MIDI %d | %.1f Hz",
							   state.lastPlayedNoteName.c_str(),
							   InstrumentSceneNamespace::getSolfeggioName(state.lastPlayedMidi), state.lastPlayedMidi,
							   state.lastPlayedFreq);
		}
		else
		{
			ImGui::TextDisabled("Last Played Note: None (Play a note on piano or keyboard)");
		}

		// Live Oscilloscope Waveform
		const auto& audioData = WeirdAudio::AudioEngine::getInstance().getAudioData();
		ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.25f, 0.92f, 1.00f, 1.00f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.06f, 0.08f, 0.11f, 1.00f));
		if (!audioData.waveform.empty() && audioData.currentVolume > 0.0005f)
		{
			ImGui::PlotLines("##InstWaveform", audioData.waveform.data(), static_cast<int>(audioData.waveform.size()),
							 0, "Live Oscilloscope (Waveform Shape)", -0.8f, 0.8f, ImVec2(-1.0f, 65.0f));
		}
		else
		{
			static const float silence[256] = {0.0f};
			ImGui::PlotLines("##InstWaveform", silence, 256, 0, "Live Oscilloscope (Play a note to see wave)", -0.8f,
							 0.8f, ImVec2(-1.0f, 65.0f));
		}
		ImGui::PopStyleColor(2);

		// 2. Sound & Timbre Parameters
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Instrument Sound & Timbre Parameters:");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Timbre is the 'material' or wave shape of the instrument (why a flute sounds different "
							  "from a violin playing the same note).");
		}

		WaveType activeWave = WaveType::SoftSine;
		const char* waveNames[] = {"Soft Sine (Warm/Pure)", "Bandlimited Saw (Bright/Buzzy)",
								   "Pulse / Square (Hollow/Retro)", "FM Pluck (Bell/Metallic)",
								   "Wavefolder (Rich/Evolving)"};
		if (state.selectedInstrument == Instrument::Lead)
		{
			int waveIdx = static_cast<int>(rack.lead);
			if (ImGui::Combo("Lead Waveform", &waveIdx, waveNames, 5))
			{
				rack.lead = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
			activeWave = rack.lead;
		}
		else if (state.selectedInstrument == Instrument::Bass)
		{
			int waveIdx = static_cast<int>(rack.bass);
			if (ImGui::Combo("Bass Waveform", &waveIdx, waveNames, 5))
			{
				rack.bass = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
			activeWave = rack.bass;
		}
		else if (state.selectedInstrument == Instrument::Pad)
		{
			int waveIdx = static_cast<int>(rack.pad);
			if (ImGui::Combo("Pad Waveform", &waveIdx, waveNames, 5))
			{
				rack.pad = static_cast<WaveType>(waveIdx);
				rackChanged = true;
			}
			activeWave = rack.pad;
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

		// Synthesis Modulation Sliders (with clear indicators for which waveform uses each slider)
		ImGui::Spacing();
		ImGui::TextDisabled("Synthesis Modulation (Affects specific Waveforms):");

		bool usesPulse =
			(state.selectedInstrument == Instrument::Lead || state.selectedInstrument == Instrument::Bass) &&
			(activeWave == WaveType::PulseSquare);
		if (!usesPulse)
			ImGui::BeginDisabled();
		if (ImGui::SliderFloat("Pulse Width (Pulse/Square)", &rack.pulseWidth, 0.05f, 0.50f, "%.2f"))
		{
			rackChanged = true;
		}
		if (!usesPulse)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Controls duty cycle of Pulse/Square wave (Lead/Bass). 0.50 = hollow square, 0.10 = thin "
							  "nasal reed.");
		}

		bool usesFM = (state.selectedInstrument == Instrument::Lead) && (activeWave == WaveType::FMPluck);
		if (!usesFM)
			ImGui::BeginDisabled();
		if (ImGui::SliderFloat("FM Mod Index (FM Pluck)", &rack.fmModIndex, 0.0f, 8.0f, "%.2f"))
		{
			rackChanged = true;
		}
		if (!usesFM)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip("Controls metallic bell strike intensity on FM Pluck waveform (Lead).");
		}

		bool usesFold = (state.selectedInstrument == Instrument::Lead || state.selectedInstrument == Instrument::Pad) &&
						(activeWave == WaveType::Wavefolder);
		if (!usesFold)
			ImGui::BeginDisabled();
		if (ImGui::SliderFloat("Wavefold Drive (Wavefolder)", &rack.foldDrive, 0.0f, 6.0f, "%.2f"))
		{
			rackChanged = true;
		}
		if (!usesFold)
			ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			ImGui::SetTooltip(
				"Folds the sine wave back on itself to create rich organ/harmonium overtones (Lead/Pad).");
		}

		if (rackChanged)
		{
			musicEngine.setInstrumentRack(rack);
		}

		// 3. Voice Shaping (Filter & Envelope)
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Voice Shaping (Filter & Envelope):");

		// Quick Voice Presets
		ImGui::TextDisabled("Presets:");
		ImGui::SameLine();
		if (ImGui::SmallButton("Warm Pluck"))
		{
			state.filterCutoff = 1800.0f;
			state.filterQ = 1.2f;
			state.noteDuration = 0.25f;
			state.noteAmp = 0.85f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Acid Squelch"))
		{
			state.filterCutoff = 950.0f;
			state.filterQ = 3.6f;
			state.noteDuration = 0.30f;
			state.noteAmp = 0.95f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Dreamy Pad"))
		{
			state.filterCutoff = 1400.0f;
			state.filterQ = 0.7f;
			state.noteDuration = 1.80f;
			state.noteAmp = 0.65f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("Bright Lead"))
		{
			state.filterCutoff = 12000.0f;
			state.filterQ = 1.4f;
			state.noteDuration = 0.50f;
			state.noteAmp = 0.85f;
		}

		ImGui::SliderFloat("Filter Cutoff", &state.filterCutoff, 100.0f, 20000.0f, "%.0f Hz",
						   ImGuiSliderFlags_Logarithmic);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Low-pass filter cutoff. Lower = muffled/underwater; Higher = bright/crisp.");
		}
		ImGui::SliderFloat("Filter Resonance (Q)", &state.filterQ, 0.5f, 4.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Boosts frequencies right at the Filter Cutoff edge. High Q + ~1000 Hz Cutoff = classic "
							  "synth 'wah' / acid squelch!");
		}
		ImGui::SliderFloat("Note Duration / Decay", &state.noteDuration, 0.05f, 3.0f, "%.2f s");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("How long the note rings out before fading to silence (staccato pluck vs long sustain).");
		}
		ImGui::SliderFloat("Note Velocity / Amp", &state.noteAmp, 0.1f, 1.5f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("How hard the key is struck (loudness).");
		}
		ImGui::SliderFloat("Stereo Pan", &state.pan, -1.0f, 1.0f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("-1.0 = Left ear, 0.0 = Center, +1.0 = Right ear.");
		}

		// 4. Scales, Solfeggio & Arpeggiator Explorer
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Music Theory & Scale Explorer:");
		ImGui::Checkbox("Show Solfeggio (Do-Re-Mi) Labels", &state.showSolfeggio);

		const char* scaleHighlightNames[] = {"None (Off)",
											 "Pentatonic Major (Happy/Open)",
											 "Pentatonic Minor (Cool/Groovy)",
											 "Major (Do-Re-Mi-Fa-Sol-La-Si)",
											 "Natural Minor (Sad/Serious)",
											 "Dorian (Jazzy/Mysterious)",
											 "Lydian (Dreamy/Sci-Fi)",
											 "Chromatic (All 12 Notes)"};
		int scaleComboIdx = state.highlightScaleIdx + 1;
		if (ImGui::Combo("Highlight Scale on Piano", &scaleComboIdx, scaleHighlightNames,
						 IM_ARRAYSIZE(scaleHighlightNames)))
		{
			state.highlightScaleIdx = scaleComboIdx - 1;
		}

		const char* rootNames[] = {"C (Do)",   "C# (Do#)", "D (Re)",	"D# (Re#)", "E (Mi)",	"F (Fa)",
								   "F# (Fa#)", "G (Sol)",  "G# (Sol#)", "A (La)",	"A# (La#)", "B (Si)"};
		ImGui::Combo("Scale Root Key", &state.highlightRootNote, rootNames, IM_ARRAYSIZE(rootNames));

		ImGui::Spacing();
		if (ImGui::Checkbox("Auto-Play Arpeggiator / Progression", &state.arpEnabled) && state.arpEnabled)
		{
			if (musicEngine.getCurrentSong())
			{
				state.arpBpm = musicEngine.getTempo();
			}
		}
		if (state.arpEnabled)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(120.0f);
			ImGui::SliderFloat("BPM", &state.arpBpm, 40.0f, 220.0f, "%.0f");
			if (musicEngine.getCurrentSong())
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Sync Song BPM"))
				{
					state.arpBpm = musicEngine.getTempo();
				}
			}
			const char* arpPatterns[] = {"Walk Scale Up", "Walk Scale Down", "Walk Up & Down",
										 "Pop Progression (I - V - vi - IV)"};
			ImGui::Combo("Arp Pattern", &state.arpPattern, arpPatterns, IM_ARRAYSIZE(arpPatterns));
		}

		// 5. Audition & Trigger Buttons
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.8f, 1.0f), "Note & Chord Audition Buttons:");

		auto playMidi = [&](int midi)
		{
			services.audio().playNote(midi, state.noteAmp, state.noteDuration, state.selectedInstrument, state.pan,
									  state.filterCutoff, state.filterQ);
			state.keyFlashTimers[midi] = 0.25f;
			state.lastPlayedMidi = midi;
			state.lastPlayedNoteName = SdfSong::midiToNoteString(midi);
			state.lastPlayedFreq = SdfSong::midiToFrequency(midi);
		};

		auto strumChord = [&](std::initializer_list<int> offsets, bool arpeggiate)
		{
			int base = (state.currentOctave + 1) * 12 + state.highlightRootNote;
			float delay = 0.0f;
			for (int off : offsets)
			{
				if (!arpeggiate || delay == 0.0f)
				{
					playMidi(base + off);
				}
				else
				{
					state.scheduledNotes.push_back({delay, base + off, state.selectedInstrument});
				}
				delay += 0.09f;
			}
		};

		static const char* noteLabels[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		static const char* solfLabels[] = {"Do",  "Do#", "Re",	 "Re#", "Mi",  "Fa",
										   "Fa#", "Sol", "Sol#", "La",	"La#", "Si"};
		int baseMidi = (state.currentOctave + 1) * 12;
		for (int i = 0; i < 12; ++i)
		{
			if (i > 0 && i % 4 != 0)
			{
				ImGui::SameLine();
			}
			char btnLabel[48];
			if (state.showSolfeggio)
			{
				std::snprintf(btnLabel, sizeof(btnLabel), "%s%d (%s)##imgui_note_%d", noteLabels[i],
							  state.currentOctave, solfLabels[i], i);
			}
			else
			{
				std::snprintf(btnLabel, sizeof(btnLabel), "%s%d##imgui_note_%d", noteLabels[i], state.currentOctave, i);
			}
			bool inScale = InstrumentSceneNamespace::isNoteInSelectedScale(baseMidi + i, state.highlightScaleIdx,
																		   state.highlightRootNote);
			if (inScale)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.28f, 1.0f));
			}
			if (ImGui::Button(btnLabel, ImVec2(state.showSolfeggio ? 84.0f : 52.0f, 28.0f)))
			{
				playMidi(baseMidi + i);
			}
			if (inScale)
			{
				ImGui::PopStyleColor();
			}
		}

		ImGui::Spacing();
		ImGui::TextDisabled("Chords (Simultaneous):");
		if (ImGui::Button("Major Triad (Happy)"))
		{
			strumChord({0, 4, 7}, false);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Root + Major 3rd (+4 semitones) + Perfect 5th (+7 semitones). E.g., Do + Mi + Sol.");
		}
		ImGui::SameLine();
		if (ImGui::Button("Minor Triad (Sad)"))
		{
			strumChord({0, 3, 7}, false);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Root + Minor 3rd (+3 semitones) + Perfect 5th (+7 semitones). Lowering the middle note "
							  "by 1 semitone makes it melancholic!");
		}
		ImGui::SameLine();
		if (ImGui::Button("Maj 7th (Dreamy)"))
		{
			strumChord({0, 4, 7, 11}, false);
		}
		ImGui::SameLine();
		if (ImGui::Button("Sus4 (Floating)"))
		{
			strumChord({0, 5, 7}, false);
		}
		ImGui::SameLine();
		if (ImGui::Button("Power 5th"))
		{
			strumChord({0, 7, 12}, false);
		}

		ImGui::TextDisabled("Strummed (Note-by-Note):");
		if (ImGui::Button("Strum Major"))
		{
			strumChord({0, 4, 7, 12}, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Strum Minor"))
		{
			strumChord({0, 3, 7, 12}, true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Strum Maj 7th"))
		{
			strumChord({0, 4, 7, 11, 12}, true);
		}

		// 5. Controls Reference Guide
		ImGui::Spacing();
		ImGui::Separator();
		if (ImGui::TreeNode("Controls & Shortcuts Reference"))
		{
			ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Gamepad Performance:");
			ImGui::BulletText("D-Pad (or Left Stick): Do (C), Re (D), Mi (E), Fa (F)");
			ImGui::BulletText(
				"Face Buttons (X, A, B, Y / West, South, East, North): Sol (G), La (A), Si (B), High Do (C+1)");
			ImGui::BulletText("Left Shoulder (LB): Hold for Sharps / Black Keys (C#, D#, F#, G#, A#)");
			ImGui::BulletText("Right Shoulder (RB): Hold for Octave +1 (Play 2nd Octave on Piano)");
			ImGui::BulletText("Left / Right Trigger (LT / RT) or Right Stick Up/Down: Octave Shift (- / +)");
			ImGui::BulletText("Start or Right Stick Left/Right: Cycle Instrument");
			ImGui::BulletText("L3 / R3 (Stick Clicks): Kick / Snare Drums");
			ImGui::BulletText("Select / Back: Return to Main Menu");

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Keyboard Shortcuts:");
			ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Gamepad Performance:");
			ImGui::BulletText("D-Pad (or Left Stick): Do (C), Re (D), Mi (E), Fa (F)");
			ImGui::BulletText(
				"Face Buttons (X, A, B, Y / West, South, East, North): Sol (G), La (A), Si (B), High Do (C+1)");
			ImGui::BulletText("Left Shoulder (LB): Hold for Sharps / Black Keys (C#, D#, F#, G#, A#)");
			ImGui::BulletText("Right Shoulder (RB): Hold for Octave +1 (Play 2nd Octave on Piano)");
			ImGui::BulletText("Left / Right Trigger (LT / RT) or Right Stick Up/Down: Octave Shift (- / +)");
			ImGui::BulletText("Start or Right Stick Left/Right: Cycle Instrument");
			ImGui::BulletText("L3 / R3 (Stick Clicks): Kick / Snare Drums");
			ImGui::BulletText("Select / Back: Return to Main Menu");

			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Keyboard Shortcuts:");
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
