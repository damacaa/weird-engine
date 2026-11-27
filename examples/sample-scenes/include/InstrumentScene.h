#pragma once

#include <array>
#include <string>
#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace InstrumentSceneNamespace
{
	struct KeyVisual
	{
		int midiNote = 60;
		bool isBlack = false;
		WeirdEngine::Entity entity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity glowDot = WeirdEngine::INVALID_ENTITY;
		float basePosX = 0.0f;
		float basePosY = 0.0f;
		float sizeX = 0.0f;
		float sizeY = 0.0f;
	};

	struct State
	{
		static constexpr size_t SCOPE_DOTS = 64;
		static constexpr size_t NUM_ACTION_BTNS = 11;

		enum ActionBtn : size_t
		{
			// Row 0 (Top HUD bar): Instrument, Sound/Wave/Kit, Octave, Scale Root, Scale Mode
			ACT_INST = 0,
			ACT_TIMBRE = 1,
			ACT_OCT = 2,
			ACT_ROOT = 3,
			ACT_SCALE = 4,
			// Row 1 (Below piano - Sound Shaping, adapts dynamically to Row 0)
			ACT_MOD = 5,
			ACT_TONE = 6,
			ACT_FLT = 7,
			// Row 2 (Below piano - Performance & Drums, with ARP toggle at col 1)
			ACT_CHORD = 8,
			ACT_KICK = 9,
			ACT_SNARE = 10
		};

		WeirdEngine::WeirdAudio::Instrument selectedInstrument = WeirdEngine::WeirdAudio::Instrument::Lead;
		int currentOctave = 4; // 1 to 7, default 4 (Middle C is C4 = MIDI 60)
		float noteDuration = 0.50f;
		float noteAmp = 0.85f;
		float filterCutoff = 12000.0f;
		float filterQ = 1.4f;
		float pan = 0.0f;

		// Interactive visual keys
		std::vector<KeyVisual> keys;
		WeirdEngine::Material2DHandle whiteKeyMat;
		WeirdEngine::Material2DHandle blackKeyMat;
		WeirdEngine::Material2DHandle whiteScaleMat;
		WeirdEngine::Material2DHandle blackScaleMat;
		WeirdEngine::Material2DHandle textWhiteMat;
		WeirdEngine::Material2DHandle accentCyanMat;
		WeirdEngine::Material2DHandle accentAmberMat;
		WeirdEngine::Material2DHandle accentGreenMat;
		WeirdEngine::Material2DHandle drumRedMat;
		WeirdEngine::Material2DHandle btnSlateMat;
		WeirdEngine::Material2DHandle textDimMat;
		WeirdEngine::Material2DHandle worldWallMat;

		// Visual press timers per MIDI note (0 to 127)
		float keyFlashTimers[128] = {0.0f};
		int lastPlayedMidi = -1;
		std::string lastPlayedNoteName = "--";
		float lastPlayedFreq = 0.0f;

		// On-screen HUD, oscilloscope, and interactive text controls
		WeirdEngine::Entity hudNoteText = WeirdEngine::INVALID_ENTITY;
		std::string cachedHudNote;

		std::array<WeirdEngine::Entity, NUM_ACTION_BTNS> actionLabels{};
		std::array<float, NUM_ACTION_BTNS> actionFlashTimers{};
		std::array<std::string, NUM_ACTION_BTNS> cachedActionLabels{};
		bool cachedModEnabled = false;
		WeirdEngine::Entity arpToggleLabel = WeirdEngine::INVALID_ENTITY;
		bool prevArpVisual = false;

		std::array<WeirdEngine::Entity, SCOPE_DOTS> scopeDots{};
		WeirdEngine::Entity worldCutout = WeirdEngine::INVALID_ENTITY;

		// Interactive Music Theory & Arpeggiator Tools
		bool showSolfeggio = true;
		int highlightScaleIdx = 2; // Default: Major (Do-Re-Mi...)
		int highlightRootNote = 0; // 0 = C (Do)

		// Scheduled notes (for Strumming Chords & Arpeggiator)
		struct ScheduledNote
		{
			float delay = 0.0f;
			int midiNote = 60;
			WeirdEngine::WeirdAudio::Instrument instrument = WeirdEngine::WeirdAudio::Instrument::Lead;
		};
		std::vector<ScheduledNote> scheduledNotes;
		bool arpEnabled = false;
		int arpPattern = 0; // 0 = Up, 1 = Down, 2 = Up-Down, 3 = Chord Progression (I-V-vi-IV)
		float arpBpm = 120.0f;
		float arpTimer = 0.0f;
		int arpStep = 0;

		// Layout caching
		int lastWidth = 0;
		int lastHeight = 0;

		// Gamepad stick selection tracking (-1 = neutral, 0..N-1 = clockwise from Up within scale)
		float stickDeadzone = 0.62f;
		int prevLeftStickDir = -1;
		int selectedStickMidi = -1;
		bool prevLeftTriggerDown = false;
		bool prevRightTriggerDown = false;
		WeirdEngine::Entity selectionDot = WeirdEngine::INVALID_ENTITY;
	};

	State& getState(WeirdEngine::Registry& registry);
	int getStickOctant(float x, float y, int prevDir, float deadzone = 0.62f);
	int getStickSector(float x, float y, int prevDir, int numSectors, float deadzone = 0.62f);
	void updateGamepadStickSelection(State& state, float leftX, float leftY, bool blackKeyMod, bool rightSideMod);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupSceneSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void inputHandlingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sequencerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void updateVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void imguiRenderSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace InstrumentSceneNamespace

class InstrumentScene : public WeirdEngine::Scene2D
{
public:
	InstrumentScene();
};
