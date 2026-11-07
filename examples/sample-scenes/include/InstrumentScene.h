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
		float basePosX = 0.0f;
		float basePosY = 0.0f;
		float sizeX = 0.0f;
		float sizeY = 0.0f;
	};

	struct State
	{
		WeirdEngine::WeirdAudio::Instrument selectedInstrument = WeirdEngine::WeirdAudio::Instrument::Lead;
		int currentOctave = 4; // 1 to 7, default 4 (Middle C is C4 = MIDI 60)
		float noteDuration = 0.45f;
		float noteAmp = 0.85f;
		float filterCutoff = 12000.0f;
		float filterQ = 1.0f;
		float pan = 0.0f;

		// Interactive visual keys
		std::vector<KeyVisual> keys;
		WeirdEngine::Material2DHandle whiteKeyMat;
		WeirdEngine::Material2DHandle blackKeyMat;
		WeirdEngine::Material2DHandle whiteScaleMat;
		WeirdEngine::Material2DHandle blackScaleMat;

		// Visual press timers per MIDI note (0 to 127)
		float keyFlashTimers[128] = {0.0f};
		int lastPlayedMidi = -1;
		std::string lastPlayedNoteName = "--";
		float lastPlayedFreq = 0.0f;

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

		// Gamepad axis tracking
		float prevStickX = 0.0f;
		float prevStickY = 0.0f;
		float prevRightStickX = 0.0f;
		float prevRightStickY = 0.0f;
		float prevLeftTrigger = 0.0f;
		float prevRightTrigger = 0.0f;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupSceneSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void inputHandlingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sequencerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void updateVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace InstrumentSceneNamespace

class InstrumentScene : public WeirdEngine::Scene2D
{
public:
	InstrumentScene();

	void onImGuiRender(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services) override;
};
