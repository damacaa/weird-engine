#include "InstrumentScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

#include "weird-renderer/components/Camera.h"
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

		// Create 2D materials for world chassis, piano keys, HUD text, and native UI buttons (12 custom + 1 default =
		// 13 of 16 slots)
		auto& worldWall = services.materials2D().createMaterial("inst_world_wall");
		worldWall.color = vec4(0.16f, 0.19f, 0.25f, 1.0f);
		state.worldWallMat = worldWall.id;

		auto& whiteMat = services.materials2D().createMaterial("inst_key_white");
		whiteMat.color = vec4(0.95f, 0.96f, 0.98f, 1.0f);
		state.whiteKeyMat = whiteMat.id;

		auto& blackMat = services.materials2D().createMaterial("inst_key_black");
		blackMat.color = vec4(0.10f, 0.12f, 0.16f, 1.0f);
		state.blackKeyMat = blackMat.id;

		auto& whiteScale = services.materials2D().createMaterial("inst_key_white_scale");
		whiteScale.color = vec4(0.75f, 0.93f, 0.98f, 1.0f);
		state.whiteScaleMat = whiteScale.id;

		auto& blackScale = services.materials2D().createMaterial("inst_key_black_scale");
		blackScale.color = vec4(0.13f, 0.42f, 0.54f, 1.0f);
		state.blackScaleMat = blackScale.id;

		auto& textWhite = services.materials2D().createMaterial("inst_text_white");
		textWhite.color = ColorPalette::White;
		state.textWhiteMat = textWhite.id;

		auto& accentCyan = services.materials2D().createMaterial("inst_accent_cyan");
		accentCyan.color = vec4(0.18f, 0.90f, 1.0f, 1.0f);
		state.accentCyanMat = accentCyan.id;

		auto& accentAmber = services.materials2D().createMaterial("inst_accent_amber");
		accentAmber.color = vec4(1.0f, 0.65f, 0.20f, 1.0f);
		state.accentAmberMat = accentAmber.id;

		auto& accentGreen = services.materials2D().createMaterial("inst_accent_green");
		accentGreen.color = vec4(0.25f, 0.95f, 0.45f, 1.0f);
		state.accentGreenMat = accentGreen.id;

		auto& drumRed = services.materials2D().createMaterial("inst_drum_red");
		drumRed.color = vec4(0.95f, 0.30f, 0.35f, 1.0f);
		state.drumRedMat = drumRed.id;

		auto& btnSlate = services.materials2D().createMaterial("inst_btn_slate");
		btnSlate.color = vec4(0.28f, 0.33f, 0.42f, 1.0f);
		state.btnSlateMat = btnSlate.id;

		auto& textDim = services.materials2D().createMaterial("inst_text_dim");
		textDim.color = vec4(0.68f, 0.76f, 0.88f, 1.0f);
		state.textDimMat = textDim.id;
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
			return "-";
		return solf[midiNote % 12];
	}

	struct InstrumentUiLayout
	{
		float winW = 800.0f;
		float winH = 800.0f;
		float startX = 30.0f;
		float startY = 260.0f;
		float whiteW = 50.0f;
		float whiteH = 210.0f;
		float blackW = 30.0f;
		float blackH = 130.0f;
		float cutLeft = 14.0f;
		float cutRight = 786.0f;
		float cutBottom = 246.0f;
		float cutTop = 504.0f;
		float scopeY = 580.0f;
		float scopeAmp = 34.0f;
		float scopeLeftX = 220.0f;
		float scopeRightX = 580.0f;
		float charSpacing = 5.0f;
		std::array<float, 3> rowY{466.0f, 118.0f, 44.0f};
		std::array<float, State::NUM_ACTION_BTNS> btnX{};
		std::array<float, State::NUM_ACTION_BTNS> btnY{};
		float arpX = 0.0f;
		float arpY = 0.0f;
		float hudNoteX = 760.0f;
		float hudNoteY = 782.0f;
	};

	static const int WHITE_OFFSETS[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23};
	static constexpr size_t NUM_WHITE_KEYS = sizeof(WHITE_OFFSETS) / sizeof(WHITE_OFFSETS[0]);

	struct BlackKeyDef
	{
		int semitone;
		int afterWhiteIdx;
	};

	static const BlackKeyDef BLACK_DEFS[] = {
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
	static constexpr size_t NUM_BLACK_KEYS = sizeof(BLACK_DEFS) / sizeof(BLACK_DEFS[0]);

	static std::string formatUiNoteName(int midiNote)
	{
		if (midiNote < 0 || midiNote > 127)
			return "-";
		// Engine bitmap font lacks '#', so display chromatic pitches cleanly as flats
		static const char* flatNames[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
		int octave = (midiNote / 12) - 1;
		return std::string(flatNames[midiNote % 12]) + std::to_string(octave);
	}

	static const char* waveShortName(WaveType w)
	{
		switch (w)
		{
			case WaveType::SoftSine:
				return "SIN";
			case WaveType::BandlimitedSaw:
				return "SAW";
			case WaveType::PulseSquare:
				return "SQR";
			case WaveType::FMPluck:
				return "FM";
			case WaveType::Wavefolder:
				return "FLD";
			default:
				return "WAV";
		}
	}

	static const char* instrumentHudTag(Instrument inst)
	{
		switch (inst)
		{
			case Instrument::Lead:
				return "LD";
			case Instrument::Bass:
				return "BS";
			case Instrument::Pad:
				return "PD";
			case Instrument::Kick:
				return "KK";
			case Instrument::Snare:
				return "SN";
			case Instrument::HiHat:
				return "HT";
			default:
				return "IN";
		}
	}

	static const char* instrumentShortName(Instrument inst)
	{
		switch (inst)
		{
			case Instrument::Lead:
				return "LEAD";
			case Instrument::Bass:
				return "BASS";
			case Instrument::Pad:
				return "PAD";
			case Instrument::Kick:
				return "KICK";
			case Instrument::Snare:
				return "SNR";
			case Instrument::HiHat:
				return "HAT";
			default:
				return "INST";
		}
	}

	static const char* scaleShortName(int scaleIdx)
	{
		static const char* names[7] = {"PMJ", "PMN", "MAJ", "MIN", "DOR", "LYD", "CHR"};
		if (scaleIdx >= 0 && scaleIdx < 7)
			return names[scaleIdx];
		return "OFF";
	}

	static const char* rootShortName(int rootNote)
	{
		static const char* names[12] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
		return names[((rootNote % 12) + 12) % 12];
	}

	static const char* drumKitShortName(int drumKit)
	{
		switch (drumKit)
		{
			case 1:
				return "WOD";
			case 2:
				return "IND";
			default:
				return "808";
		}
	}

	static bool isMelodicInstrument(Instrument inst)
	{
		return inst == Instrument::Lead || inst == Instrument::Bass || inst == Instrument::Pad;
	}

	struct VoiceTonePreset
	{
		const char* shortName;
		float cutoff;
		float q;
		float duration;
		float amp;
	};

	static constexpr std::array<VoiceTonePreset, 4> VOICE_PRESETS = {{
		{"BRT", 12000.0f, 1.4f, 0.50f, 0.85f},
		{"PLK", 1800.0f, 1.2f, 0.25f, 0.85f},
		{"ACD", 950.0f, 3.6f, 0.30f, 0.95f},
		{"PAD", 1400.0f, 0.7f, 1.80f, 0.65f},
	}};

	struct CutoffPreset
	{
		const char* label;
		float hz;
	};

	static constexpr std::array<CutoffPreset, 5> CUTOFF_PRESETS = {{
		{"12K", 12000.0f},
		{"4.0K", 4000.0f},
		{"1.8K", 1800.0f},
		{"950", 950.0f},
		{"450", 450.0f},
	}};

	static constexpr std::array<float, 4> PULSE_STEPS = {0.50f, 0.35f, 0.20f, 0.10f};
	static constexpr std::array<float, 5> FM_STEPS = {0.5f, 2.0f, 4.0f, 6.0f, 8.0f};
	static constexpr std::array<float, 4> FOLD_STEPS = {1.0f, 2.2f, 3.8f, 5.5f};

	template <size_t N> static int nearestFloatIdx(const std::array<float, N>& values, float target)
	{
		int bestIdx = 0;
		float bestDist = std::abs(values[0] - target);
		for (size_t i = 1; i < N; ++i)
		{
			float d = std::abs(values[i] - target);
			if (d < bestDist)
			{
				bestDist = d;
				bestIdx = static_cast<int>(i);
			}
		}
		return bestIdx;
	}

	static int matchingTonePresetIdx(const State& state)
	{
		for (size_t i = 0; i < VOICE_PRESETS.size(); ++i)
		{
			const auto& p = VOICE_PRESETS[i];
			if (std::abs(state.filterCutoff - p.cutoff) < 20.0f && std::abs(state.filterQ - p.q) < 0.08f &&
				std::abs(state.noteDuration - p.duration) < 0.03f && std::abs(state.noteAmp - p.amp) < 0.03f)
			{
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	static void applyTonePreset(State& state, int idx)
	{
		int n = static_cast<int>(VOICE_PRESETS.size());
		const auto& p = VOICE_PRESETS[((idx % n) + n) % n];
		state.filterCutoff = p.cutoff;
		state.filterQ = p.q;
		state.noteDuration = p.duration;
		state.noteAmp = p.amp;
	}

	static int matchingCutoffIdx(float cutoffHz)
	{
		int best = 0;
		float bestRatio = std::abs(std::log2((std::max)(cutoffHz, 20.0f) / CUTOFF_PRESETS[0].hz));
		for (size_t i = 1; i < CUTOFF_PRESETS.size(); ++i)
		{
			float r = std::abs(std::log2((std::max)(cutoffHz, 20.0f) / CUTOFF_PRESETS[i].hz));
			if (r < bestRatio)
			{
				bestRatio = r;
				best = static_cast<int>(i);
			}
		}
		return best;
	}

	static void triggerInstrumentMidi(State& state, ServiceProvider& services, int midiNote, Instrument inst)
	{
		if (midiNote < 0 || midiNote > 127)
			return;
		services.audio().playNote(midiNote, state.noteAmp, state.noteDuration, inst, state.pan, state.filterCutoff,
								  state.filterQ);
		state.keyFlashTimers[midiNote] = 0.25f;
		state.lastPlayedMidi = midiNote;
		state.lastPlayedNoteName = SdfSong::midiToNoteString(midiNote);
		state.lastPlayedFreq = SdfSong::midiToFrequency(midiNote);
	}

	static void cycleActiveInstrument(State& state, int delta)
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
		int n = static_cast<int>(SongInstruments.size());
		state.selectedInstrument = SongInstruments[((cur + delta) % n + n) % n];
	}

	static void cycleTimbreInInstrument(State& state, ServiceProvider& services, int delta)
	{
		auto& music = services.audio().music();
		if (isMelodicInstrument(state.selectedInstrument))
		{
			int cur = static_cast<int>(music.getInstrumentWaveType(state.selectedInstrument));
			constexpr int numWaves = 5;
			int next = ((cur + delta) % numWaves + numWaves) % numWaves;
			music.setInstrumentWaveType(state.selectedInstrument, static_cast<WaveType>(next));
		}
		else
		{
			auto rack = music.getInstrumentRack();
			constexpr int numKits = 3;
			rack.drumKit = ((rack.drumKit + delta) % numKits + numKits) % numKits;
			music.setInstrumentRack(rack);
		}
	}

	static bool isModControlEnabled(const State& state, const SdfMusicEngine& music)
	{
		if (!isMelodicInstrument(state.selectedInstrument))
		{
			return true; // HIT button previews active drum
		}
		const WaveType w = music.getInstrumentWaveType(state.selectedInstrument);
		return w == WaveType::PulseSquare || w == WaveType::FMPluck || w == WaveType::Wavefolder;
	}

	static void stepModControl(State& state, ServiceProvider& services, int delta)
	{
		if (!isMelodicInstrument(state.selectedInstrument))
		{
			int midi = (state.selectedInstrument == Instrument::Snare)	 ? 38
					   : (state.selectedInstrument == Instrument::HiHat) ? 42
																		 : 36;
			triggerInstrumentMidi(state, services, midi, state.selectedInstrument);
			return;
		}
		auto& music = services.audio().music();
		auto rack = music.getInstrumentRack();
		const WaveType w = music.getInstrumentWaveType(state.selectedInstrument);
		if (w == WaveType::PulseSquare)
		{
			int n = static_cast<int>(PULSE_STEPS.size());
			int idx = nearestFloatIdx(PULSE_STEPS, rack.pulseWidth);
			rack.pulseWidth = PULSE_STEPS[((idx + delta) % n + n) % n];
			music.setInstrumentRack(rack);
		}
		else if (w == WaveType::FMPluck)
		{
			int n = static_cast<int>(FM_STEPS.size());
			int idx = nearestFloatIdx(FM_STEPS, rack.fmModIndex);
			rack.fmModIndex = FM_STEPS[((idx + delta) % n + n) % n];
			music.setInstrumentRack(rack);
		}
		else if (w == WaveType::Wavefolder)
		{
			int n = static_cast<int>(FOLD_STEPS.size());
			int idx = nearestFloatIdx(FOLD_STEPS, rack.foldDrive);
			rack.foldDrive = FOLD_STEPS[((idx + delta) % n + n) % n];
			music.setInstrumentRack(rack);
		}
	}

	static std::string buildActionLabel(const State& state, const SdfMusicEngine& music, size_t btnIdx)
	{
		const auto rack = music.getInstrumentRack();
		char buf[16];
		switch (btnIdx)
		{
			case State::ACT_INST:
				return instrumentHudTag(state.selectedInstrument);
			case State::ACT_TIMBRE:
				if (isMelodicInstrument(state.selectedInstrument))
					return waveShortName(music.getInstrumentWaveType(state.selectedInstrument));
				return drumKitShortName(rack.drumKit);
			case State::ACT_OCT:
				return std::string("O") + std::to_string(state.currentOctave);
			case State::ACT_ROOT:
				return rootShortName(state.highlightRootNote);
			case State::ACT_SCALE:
				return scaleShortName(state.highlightScaleIdx);
			case State::ACT_MOD:
				if (!isMelodicInstrument(state.selectedInstrument))
					return std::string("H:") + instrumentHudTag(state.selectedInstrument);
				switch (music.getInstrumentWaveType(state.selectedInstrument))
				{
					case WaveType::PulseSquare:
						std::snprintf(buf, sizeof(buf), "PW%d", static_cast<int>(std::round(rack.pulseWidth * 100.0f)));
						return buf;
					case WaveType::FMPluck:
						std::snprintf(buf, sizeof(buf), "FM:%d", static_cast<int>(std::round(rack.fmModIndex)));
						return buf;
					case WaveType::Wavefolder:
						std::snprintf(buf, sizeof(buf), "FL:%d", static_cast<int>(std::round(rack.foldDrive)));
						return buf;
					default:
						return "-";
				}
			case State::ACT_TONE:
			{
				int presetIdx = matchingTonePresetIdx(state);
				return std::string("T:") + (presetIdx >= 0 ? VOICE_PRESETS[presetIdx].shortName : "CST");
			}
			case State::ACT_FLT:
				return std::string("F:") + CUTOFF_PRESETS[matchingCutoffIdx(state.filterCutoff)].label;
			case State::ACT_CHORD:
				return "CHRD";
			case State::ACT_KICK:
				return "KICK";
			case State::ACT_SNARE:
				return "SNR";
			default:
				return "";
		}
	}

	static void triggerDefaultChord(State& state)
	{
		int rootMidi = (state.currentOctave + 1) * 12 + state.highlightRootNote;
		bool minorThird =
			(state.highlightScaleIdx == 1 || state.highlightScaleIdx == 3 || state.highlightScaleIdx == 4);
		int third = minorThird ? 3 : 4;
		const int offsets[4] = {0, third, 7, 12};
		for (size_t i = 0; i < 4; ++i)
		{
			state.scheduledNotes.push_back(
				{static_cast<float>(i) * 0.045f, rootMidi + offsets[i], state.selectedInstrument});
		}
	}

	static void triggerAction(State& state, ServiceProvider& services, size_t act, int delta, bool playFeedbackSound,
							  bool clampOctave = false)
	{
		if (act >= State::NUM_ACTION_BTNS)
			return;
		state.actionFlashTimers[act] = 0.22f;
		switch (act)
		{
			case State::ACT_INST:
				cycleActiveInstrument(state, delta);
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 680.0f, false, vec3(0.0f), 1});
				break;
			case State::ACT_TIMBRE:
				cycleTimbreInInstrument(state, services, delta);
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 640.0f, false, vec3(0.0f), 1});
				break;
			case State::ACT_OCT:
				state.currentOctave = clampOctave ? std::clamp(state.currentOctave + delta, 1, 7)
												  : (((state.currentOctave - 1 + delta) % 7 + 7) % 7 + 1);
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 520.0f, false, vec3(0.0f), 1});
				break;
			case State::ACT_ROOT:
				state.highlightRootNote = ((state.highlightRootNote + delta) % 12 + 12) % 12;
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 660.0f, false, vec3(0.0f), 1});
				break;
			case State::ACT_SCALE:
			{
				// Cycle OFF (-1) and MusicalScales (0..6) = 8 states total
				int mapped = state.highlightScaleIdx + 1;
				mapped = ((mapped + delta) % 8 + 8) % 8;
				state.highlightScaleIdx = mapped - 1;
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 620.0f, false, vec3(0.0f), 1});
				break;
			}
			case State::ACT_MOD:
				stepModControl(state, services, delta);
				break;
			case State::ACT_TONE:
			{
				int cur = matchingTonePresetIdx(state);
				int next = (cur < 0) ? 0 : (cur + delta);
				applyTonePreset(state, next);
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 600.0f, false, vec3(0.0f), 1});
				break;
			}
			case State::ACT_FLT:
			{
				int n = static_cast<int>(CUTOFF_PRESETS.size());
				int cur = matchingCutoffIdx(state.filterCutoff);
				int next = ((cur + delta) % n + n) % n;
				state.filterCutoff = CUTOFF_PRESETS[next].hz;
				if (playFeedbackSound)
					services.audio().playSound({0.02f, 520.0f, false, vec3(0.0f), 1});
				break;
			}
			case State::ACT_CHORD:
				triggerDefaultChord(state);
				break;
			case State::ACT_KICK:
				triggerInstrumentMidi(state, services, 36, Instrument::Kick);
				break;
			case State::ACT_SNARE:
				triggerInstrumentMidi(state, services, 38, Instrument::Snare);
				break;
			default:
				break;
		}
	}

	static InstrumentUiLayout computeInstrumentLayout()
	{
		InstrumentUiLayout l;
		l.winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		l.winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		l.charSpacing = (l.winW >= 720.0f) ? 5.0f : 3.5f;

		float keyboardW = (std::min)(l.winW - 44.0f, 740.0f);
		l.whiteW = keyboardW / static_cast<float>(NUM_WHITE_KEYS);
		l.whiteH = (l.winH < 560.0f) ? (std::min)(176.0f, l.winH * 0.36f) : (std::min)(210.0f, l.winH * 0.39f);
		l.blackW = l.whiteW * 0.60f;
		l.blackH = l.whiteH * 0.62f;
		l.startX = (l.winW - keyboardW) * 0.5f;
		l.startY = (l.winH < 560.0f) ? 172.0f : (l.winH * 0.43f - l.whiteH * 0.5f);

		const float cutPadX = (l.winW >= 760.0f) ? 18.0f : 12.0f;
		const float cutPadY = (l.winH >= 560.0f) ? 16.0f : 10.0f;
		l.cutLeft = l.startX - cutPadX;
		l.cutRight = l.startX + keyboardW + cutPadX;
		l.cutBottom = l.startY - cutPadY;
		l.cutTop = l.startY + l.whiteH + cutPadY;

		float topHudFloor = l.winH - 58.0f;
		float pianoTop = l.startY + l.whiteH;
		l.scopeY = pianoTop + (topHudFloor - pianoTop) * 0.50f;
		l.scopeAmp = (std::min)(36.0f, (std::max)(16.0f, (topHudFloor - pianoTop) * 0.28f));
		const float scopeW = (std::min)(l.winW * 0.48f, 360.0f);
		l.scopeLeftX = (l.winW - scopeW) * 0.5f;
		l.scopeRightX = l.scopeLeftX + scopeW;

		const float topY = (l.winH < 560.0f) ? (l.winH - 14.0f) : (l.winH - 16.0f);
		if (l.winH < 560.0f)
		{
			l.rowY = {topY, 118.0f, 44.0f};
		}
		else
		{
			l.rowY = {topY, l.startY - 62.0f, l.startY - 126.0f};
		}

		// Row 0 (Top HUD bar): reuse top-left (INST, TIMBRE, OCT) and top-right (ROOT, SCALE, Note) cleanly
		const float charStep = 30.0f + l.charSpacing;
		const float leftEdge = 16.0f;
		const float rightEdge = l.winW - 16.0f;
		l.btnX[State::ACT_INST] = leftEdge + 1.0f * charStep;
		l.btnY[State::ACT_INST] = topY;
		l.btnX[State::ACT_TIMBRE] = leftEdge + 4.1f * charStep;
		l.btnY[State::ACT_TIMBRE] = topY;
		l.btnX[State::ACT_OCT] = leftEdge + 7.2f * charStep;
		l.btnY[State::ACT_OCT] = topY;

		l.hudNoteX = rightEdge - 1.5f * charStep;
		l.hudNoteY = topY;
		l.btnX[State::ACT_SCALE] = rightEdge - 5.1f * charStep;
		l.btnY[State::ACT_SCALE] = topY;
		l.btnX[State::ACT_ROOT] = rightEdge - 8.1f * charStep;
		l.btnY[State::ACT_ROOT] = topY;

		// Rows 1 & 2 (Below piano): clamped container width centered directly under the piano
		const float minMarginX = (l.winW >= 760.0f) ? 40.0f : 14.0f;
		const float maxControlsW = (l.winW >= 760.0f) ? (std::min)(keyboardW, 660.0f) : (l.winW - minMarginX * 2.0f);
		const float usableW = (std::min)(l.winW - minMarginX * 2.0f, maxControlsW);
		const float marginX = (l.winW - usableW) * 0.5f;

		auto placeCenteredRow = [&](int row, std::initializer_list<int> slots)
		{
			int count = static_cast<int>(slots.size());
			int c = 0;
			for (int act : slots)
			{
				float x = marginX + ((static_cast<float>(c) + 0.5f) / static_cast<float>(count)) * usableW;
				if (act < 0)
				{
					l.arpX = x;
					l.arpY = l.rowY[row];
				}
				else
				{
					l.btnX[act] = x;
					l.btnY[act] = l.rowY[row];
				}
				++c;
			}
		};

		placeCenteredRow(1, {State::ACT_MOD, State::ACT_TONE, State::ACT_FLT});
		placeCenteredRow(2, {State::ACT_CHORD, -1, State::ACT_KICK, State::ACT_SNARE});
		return l;
	}

	static Material2DHandle scopeAccentMaterial(const State& state, Instrument inst)
	{
		switch (inst)
		{
			case Instrument::Bass:
				return state.accentAmberMat;
			case Instrument::Pad:
				return state.accentGreenMat;
			case Instrument::Kick:
			case Instrument::Snare:
			case Instrument::HiHat:
				return state.drumRedMat;
			default:
				return state.accentCyanMat;
		}
	}

	static void updateWorldCutout(Registry& registry, const State& state, ServiceProvider& services,
								  const InstrumentUiLayout& l)
	{
		if (!registry.isEntityValid(state.worldCutout) || !registry.hasComponent<Shape>(state.worldCutout))
		{
			return;
		}

		const auto& camTf = registry.getComponent<Transform>(services.render().getCameraEntity());
		// UI and Camera::screenPositionToWorldPosition2D both use Y-up coordinates (0 at bottom)
		const vec2 worldA = ECS::Camera::screenPositionToWorldPosition2D(camTf, vec2(l.cutLeft, l.cutBottom));
		const vec2 worldB = ECS::Camera::screenPositionToWorldPosition2D(camTf, vec2(l.cutRight, l.cutTop));
		const float sizeX = 0.5f * std::abs(worldB.x - worldA.x);
		const float sizeY = 0.5f * std::abs(worldB.y - worldA.y);

		auto& cut = registry.getComponent<Shape>(state.worldCutout);
		cut.parameters[DefaultShapes::RoundedBox::PosX] = 0.5f * (worldA.x + worldB.x);
		cut.parameters[DefaultShapes::RoundedBox::PosY] = 0.5f * (worldA.y + worldB.y);
		cut.parameters[DefaultShapes::RoundedBox::SizeX] = sizeX;
		cut.parameters[DefaultShapes::RoundedBox::SizeY] = sizeY;
		cut.parameters[DefaultShapes::RoundedBox::Radius] = (std::min)(0.45f, 0.08f * (std::min)(sizeX, sizeY));
		registry.setComponentDirty(cut);
	}

	static void relayoutScene(Registry& registry, State& state, ServiceProvider& services)
	{
		const InstrumentUiLayout l = computeInstrumentLayout();

		auto& uiCtx = services.render().getContextUI();
		if (std::abs(uiCtx.charSpacing - l.charSpacing) > 0.01f)
		{
			uiCtx.charSpacing = l.charSpacing;
			auto markDirtyIfValid = [&](Entity e)
			{
				if (registry.isEntityValid(e) && registry.hasComponent<UITextRenderer>(e))
				{
					registry.setComponentDirty(registry.getComponent<UITextRenderer>(e));
				}
			};
			markDirtyIfValid(state.hudNoteText);
			markDirtyIfValid(state.arpToggleLabel);
			for (size_t i = 0; i < State::NUM_ACTION_BTNS; ++i)
			{
				markDirtyIfValid(state.actionLabels[i]);
			}
		}

		updateWorldCutout(registry, state, services, l);

		for (size_t i = 0; i < state.keys.size(); ++i)
		{
			auto& kv = state.keys[i];
			if (!kv.isBlack)
			{
				size_t wIdx = i;
				kv.basePosX = l.startX + static_cast<float>(wIdx) * l.whiteW + l.whiteW * 0.5f;
				kv.basePosY = l.startY + l.whiteH * 0.5f;
				kv.sizeX = (l.whiteW - 3.0f) * 0.5f;
				kv.sizeY = (l.whiteH - 3.0f) * 0.5f;
			}
			else
			{
				size_t bIdx = i - NUM_WHITE_KEYS;
				const auto& bd = BLACK_DEFS[bIdx];
				kv.basePosX = l.startX + static_cast<float>(bd.afterWhiteIdx + 1) * l.whiteW;
				kv.basePosY = l.startY + l.whiteH - l.blackH * 0.5f;
				kv.sizeX = l.blackW * 0.5f;
				kv.sizeY = l.blackH * 0.5f;
			}

			if (registry.isEntityValid(kv.entity) && registry.hasComponent<UIShape>(kv.entity))
			{
				auto& s = registry.getComponent<UIShape>(kv.entity);
				s.parameters[DefaultShapes::RoundBox::PosX] = kv.basePosX;
				s.parameters[DefaultShapes::RoundBox::PosY] = kv.basePosY;
				s.parameters[DefaultShapes::RoundBox::SizeX] = kv.sizeX;
				s.parameters[DefaultShapes::RoundBox::SizeY] = kv.sizeY;
			}
		}

		if (registry.isEntityValid(state.hudNoteText) && registry.hasComponent<Transform>(state.hudNoteText))
		{
			registry.getComponent<Transform>(state.hudNoteText).position = vec3(l.hudNoteX, l.hudNoteY, 0.0f);
		}

		for (size_t i = 0; i < State::NUM_ACTION_BTNS; ++i)
		{
			if (registry.isEntityValid(state.actionLabels[i]) &&
				registry.hasComponent<Transform>(state.actionLabels[i]))
			{
				registry.getComponent<Transform>(state.actionLabels[i]).position = vec3(l.btnX[i], l.btnY[i], 0.0f);
			}
		}

		if (registry.isEntityValid(state.arpToggleLabel) && registry.hasComponent<Transform>(state.arpToggleLabel))
		{
			registry.getComponent<Transform>(state.arpToggleLabel).position = vec3(l.arpX, l.arpY, 0.0f);
		}
	}

	void setupSceneSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.render().getContextUI().charSpacing = 7.5f;
		state.lastWidth = Display::width;
		state.lastHeight = Display::height;

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 0.0f, 25.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Grid;
		background.primaryColor = vec4(0.06f, 0.08f, 0.13f, 1.0f);
		background.secondaryColor = vec4(0.14f, 0.21f, 0.31f, 1.0f);
		background.scale = 2.5f;
		background.intensity = 1.0f;
		background.parallax = 0.0f;

		const InstrumentUiLayout l = computeInstrumentLayout();

		// Large 2D chassis plate with a rounded cutout framing the piano + padding
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 0.0f},
												  {DefaultShapes::Box::PosY, -100.0f},
												  {DefaultShapes::Box::SizeX, 150.0f},
												  {DefaultShapes::Box::SizeY, 100.0f}},
									.material = state.worldWallMat,
									.combination = CombinationType::Addition,
									.hasCollision = false});

		state.worldCutout = services.shapes().addShape({.shapeId = DefaultShapes::RoundedBox,
														.variables = {{DefaultShapes::RoundedBox::PosX, 0.0f},
																	  {DefaultShapes::RoundedBox::PosY, 0.0f},
																	  {DefaultShapes::RoundedBox::SizeX, 10.0f},
																	  {DefaultShapes::RoundedBox::SizeY, 4.0f},
																	  {DefaultShapes::RoundedBox::Radius, 0.35f}},
														.material = state.worldWallMat,
														.combination = CombinationType::Subtraction,
														.hasCollision = false});

		state.keys.clear();
		state.keys.reserve(NUM_WHITE_KEYS + NUM_BLACK_KEYS);

		auto createKeyDot = [&]()
		{
			Entity dot = registry.createEntity();
			registry.addComponent<Transform>(dot).position = vec3(-1000.0f, -1000.0f, 0.0f);
			auto& uiDot = registry.addComponent<UIDot>(dot);
			uiDot.materialId = state.accentCyanMat.id;
			return dot;
		};

		// First create white keys (group 0)
		for (size_t i = 0; i < NUM_WHITE_KEYS; ++i)
		{
			float kx = l.startX + static_cast<float>(i) * l.whiteW + l.whiteW * 0.5f;
			float ky = l.startY + l.whiteH * 0.5f;

			auto entity =
				services.shapes().addUIShape({.shapeId = DefaultShapes::RoundBox,
											  .variables = {{DefaultShapes::RoundBox::PosX, kx},
															{DefaultShapes::RoundBox::PosY, ky},
															{DefaultShapes::RoundBox::SizeX, (l.whiteW - 3.0f) * 0.5f},
															{DefaultShapes::RoundBox::SizeY, (l.whiteH - 3.0f) * 0.5f},
															{DefaultShapes::RoundBox::Radius, 4.0f}},
											  .material = state.whiteKeyMat,
											  .group = 0});
			auto& btn = registry.addComponent<ShapeButton>(entity);
			btn.clickPadding = 0.0f;

			KeyVisual kv;
			kv.midiNote = 60 + WHITE_OFFSETS[i];
			kv.isBlack = false;
			kv.entity = entity;
			kv.glowDot = createKeyDot();
			kv.basePosX = kx;
			kv.basePosY = ky;
			kv.sizeX = (l.whiteW - 3.0f) * 0.5f;
			kv.sizeY = (l.whiteH - 3.0f) * 0.5f;
			state.keys.push_back(kv);
		}

		// Then create black keys along the top edge (group 1 renders cleanly above white keys)
		for (size_t i = 0; i < NUM_BLACK_KEYS; ++i)
		{
			const auto& bd = BLACK_DEFS[i];
			float kx = l.startX + static_cast<float>(bd.afterWhiteIdx + 1) * l.whiteW;
			float ky = l.startY + l.whiteH - l.blackH * 0.5f;

			auto entity = services.shapes().addUIShape({.shapeId = DefaultShapes::RoundBox,
														.variables = {{DefaultShapes::RoundBox::PosX, kx},
																	  {DefaultShapes::RoundBox::PosY, ky},
																	  {DefaultShapes::RoundBox::SizeX, l.blackW * 0.5f},
																	  {DefaultShapes::RoundBox::SizeY, l.blackH * 0.5f},
																	  {DefaultShapes::RoundBox::Radius, 3.0f}},
														.material = state.blackKeyMat,
														.group = 1});
			auto& btn = registry.addComponent<ShapeButton>(entity);
			btn.clickPadding = 0.0f;

			KeyVisual kv;
			kv.midiNote = 60 + bd.semitone;
			kv.isBlack = true;
			kv.entity = entity;
			kv.glowDot = createKeyDot();
			kv.basePosX = kx;
			kv.basePosY = ky;
			kv.sizeX = l.blackW * 0.5f;
			kv.sizeY = l.blackH * 0.5f;
			state.keys.push_back(kv);
		}

		// Selection dot rendered above both white (group 0) and black (group 1) piano keys
		state.selectionDot = services.shapes().addUIShape({.shapeId = DefaultShapes::Circle,
														   .variables = {{DefaultShapes::Circle::PosX, -10000.0f},
																		 {DefaultShapes::Circle::PosY, -10000.0f},
																		 {DefaultShapes::Circle::Radius, 0.0f}},
														   .material = state.accentAmberMat,
														   .group = 2});

		// Top-right last played note readout
		{
			state.hudNoteText = registry.createEntity();
			registry.addComponent<Transform>(state.hudNoteText).position = vec3(l.hudNoteX, l.hudNoteY, 0.0f);
			auto& rtr = registry.addComponent<UITextRenderer>(state.hudNoteText);
			rtr.text = "-";
			rtr.material = state.accentCyanMat.id;
			rtr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			rtr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
			state.cachedHudNote = "-";
		}

		// Live audio oscilloscope dot ribbon (tight spacing for smooth SDF continuity)
		{
			for (size_t i = 0; i < State::SCOPE_DOTS; ++i)
			{
				Entity dot = registry.createEntity();
				float frac = static_cast<float>(i) / static_cast<float>(State::SCOPE_DOTS - 1);
				float x = l.scopeLeftX + (l.scopeRightX - l.scopeLeftX) * frac;
				registry.addComponent<Transform>(dot).position = vec3(x, l.scopeY, 0.0f);
				registry.addComponent<UIDot>(dot).materialId = state.accentCyanMat.id;
				state.scopeDots[i] = dot;
			}
		}

		// Interactive text controls across 3 logical rows:
		// Row 0 (Top HUD): Instrument, Wave/Kit, Octave, Scale Root, Scale Mode
		// Row 1 (Below Piano - Sound Shaping): Mod/Hit, Tone Preset, Filter Cutoff
		// Row 2 (Below Piano - Performance): Chord, Arp, Kick, Snare
		const auto& music = services.audio().music();
		state.cachedModEnabled = isModControlEnabled(state, music);
		const Material2DHandle actionMats[State::NUM_ACTION_BTNS] = {
			scopeAccentMaterial(state, state.selectedInstrument),
			state.accentAmberMat,
			state.textWhiteMat,
			state.accentGreenMat,
			state.accentGreenMat,
			state.cachedModEnabled ? state.accentAmberMat : state.textDimMat,
			state.accentAmberMat,
			state.accentAmberMat,
			state.accentCyanMat,
			state.drumRedMat,
			state.drumRedMat};

		for (size_t i = 0; i < State::NUM_ACTION_BTNS; ++i)
		{
			std::string initialLabel = buildActionLabel(state, music, i);
			state.cachedActionLabels[i] = initialLabel;
			const bool isTopRow = (i <= State::ACT_SCALE);

			state.actionLabels[i] = registry.createEntity();
			registry.addComponent<Transform>(state.actionLabels[i]).position = vec3(l.btnX[i], l.btnY[i], 0.0f);
			auto& tr = registry.addComponent<UITextRenderer>(state.actionLabels[i]);
			tr.text = initialLabel;
			tr.material = actionMats[i].id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			tr.verticalAlignment =
				isTopRow ? TextRenderer::VerticalAlignment::Top : TextRenderer::VerticalAlignment::Center;
			auto& txtBtn = registry.addComponent<TextButton>(state.actionLabels[i]);
			txtBtn.clickPadding = 8.0f;
			txtBtn.hoverMaterial = state.textWhiteMat.id;
		}

		// Arpeggiator TextToggle (Row 2 col 1)
		{
			state.prevArpVisual = state.arpEnabled;
			state.arpToggleLabel = registry.createEntity();
			registry.addComponent<Transform>(state.arpToggleLabel).position = vec3(l.arpX, l.arpY, 0.0f);
			auto& tr = registry.addComponent<UITextRenderer>(state.arpToggleLabel);
			tr.text = "ARP";
			tr.material = state.arpEnabled ? state.accentGreenMat.id : state.textDimMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
			auto& txtTgl = registry.addComponent<TextToggle>(state.arpToggleLabel);
			txtTgl.active = state.arpEnabled;
			txtTgl.clickPadding = 8.0f;
			txtTgl.activeMaterial = state.accentGreenMat.id;
			txtTgl.inactiveMaterial = state.textDimMat.id;
		}

		relayoutScene(registry, state, services);
	}

	int getStickSector(float x, float y, int prevDir, int numSectors, float deadzone)
	{
		if (numSectors <= 0)
		{
			return -1;
		}
		const float engageR = std::clamp(deadzone, 0.15f, 0.90f);
		const float releaseR = (std::max)(0.12f, engageR - 0.12f);
		const float rSq = x * x + y * y;
		if (rSq < releaseR * releaseR)
		{
			return -1;
		}
		constexpr float TWO_PI = 6.28318530717958647692f;
		float angle = std::atan2(x, -y); // 0 = Up, clockwise
		if (angle < 0.0f)
		{
			angle += TWO_PI;
		}
		const float sectorAngle = TWO_PI / static_cast<float>(numSectors);
		int sector = static_cast<int>(std::floor((angle + sectorAngle * 0.5f) / sectorAngle)) % numSectors;
		if (rSq < engageR * engageR)
		{
			// Inside deadzone hysteresis band: keep same note, reject snapback into another sector
			return (prevDir >= 0 && sector == prevDir) ? prevDir : -1;
		}
		return sector;
	}

	int getStickOctant(float x, float y, int prevDir, float deadzone)
	{
		return getStickSector(x, y, prevDir, 8, deadzone);
	}

	void updateGamepadStickSelection(State& state, float leftX, float leftY, bool blackKeyMod, bool rightSideMod)
	{
		const int baseMidi = (state.currentOctave + 1) * 12;
		const int octaveShift = rightSideMod ? 12 : 0;

		std::array<int, 8> candidates{};
		int count = 0;

		auto tryAddOffset = [&](int offset)
		{
			if (count >= static_cast<int>(candidates.size()))
				return;
			const int midi = baseMidi + octaveShift + offset;
			if (state.highlightScaleIdx < 0 ||
				isNoteInSelectedScale(midi, state.highlightScaleIdx, state.highlightRootNote))
			{
				candidates[count++] = midi;
			}
		};

		if (blackKeyMod)
		{
			for (size_t i = 0; i < 5; ++i)
			{
				tryAddOffset(BLACK_DEFS[i].semitone);
			}
		}
		else
		{
			for (size_t i = 0; i < 7; ++i)
			{
				tryAddOffset(WHITE_OFFSETS[i]);
			}
		}

		int dir = getStickSector(leftX, leftY, state.prevLeftStickDir, count, state.stickDeadzone);
		state.prevLeftStickDir = dir;
		state.selectedStickMidi = (dir >= 0 && dir < count) ? candidates[dir] : -1;
	}

	void inputHandlingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		auto& input = services.input();
		auto& music = services.audio().music();

		// Read gamepad axes (Right Stick unused)
		float stickX = input.getGamepadAxis(Input::GamepadAxis::LeftX);
		float stickY = input.getGamepadAxis(Input::GamepadAxis::LeftY);
		float leftTrigger = input.getGamepadAxis(Input::GamepadAxis::LeftTrigger);
		float rightTrigger = input.getGamepadAxis(Input::GamepadAxis::RightTrigger);

		// 1. Instrument Selection (1-6, TAB, L2/R2) & Timbre Shortcut (Start)
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
			triggerAction(state, services, State::ACT_INST, 1, false);
		}

		// Gamepad direct 1-button-per-function controls (no D-pad menu cursor):
		// L2 / R2: Cycle Active Instrument / Wave-Kit
		bool leftTriggerDown = state.prevLeftTriggerDown ? (leftTrigger > 0.35f) : (leftTrigger > 0.5f);
		if (leftTriggerDown && !state.prevLeftTriggerDown)
		{
			triggerAction(state, services, State::ACT_INST, 1, true);
		}
		state.prevLeftTriggerDown = leftTriggerDown;

		bool rightTriggerDown = state.prevRightTriggerDown ? (rightTrigger > 0.35f) : (rightTrigger > 0.5f);
		if (rightTriggerDown && !state.prevRightTriggerDown)
		{
			triggerAction(state, services, State::ACT_TIMBRE, 1, true);
		}
		state.prevRightTriggerDown = rightTriggerDown;

		// D-Pad Left / Right: Octave -1 / +1 (clamped 1..7)
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadLeft))
		{
			triggerAction(state, services, State::ACT_OCT, -1, true, true);
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadRight))
		{
			triggerAction(state, services, State::ACT_OCT, 1, true, true);
		}

		// D-Pad Up / Down: Cycle Tone Preset / Filter Cutoff
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadUp))
		{
			triggerAction(state, services, State::ACT_TONE, 1, true);
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::DpadDown))
		{
			triggerAction(state, services, State::ACT_FLT, 1, true);
		}

		// Start: Cycle Mod / Trigger Drum Hit
		if (input.getGamepadButtonDown(Input::GamepadButton::Start))
		{
			triggerAction(state, services, State::ACT_MOD, 1, true);
		}

		// 2. Keyboard Octave Shift (Z / X or Up / Down)
		if (input.getKeyDown(Input::Z) || input.getKeyDown(Input::Down))
		{
			state.currentOctave = (std::max)(1, state.currentOctave - 1);
			state.actionFlashTimers[State::ACT_OCT] = 0.22f;
		}
		if (input.getKeyDown(Input::X) || input.getKeyDown(Input::Up))
		{
			state.currentOctave = (std::min)(7, state.currentOctave + 1);
			state.actionFlashTimers[State::ACT_OCT] = 0.22f;
		}

		auto playNoteMidi = [&](int midiNote, Instrument inst)
		{ triggerInstrumentMidi(state, services, midiNote, inst); };

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

		// 4. Gamepad Left-Stick Scale Note Selection & Face Button Performance
		// Modifiers: L1 = Black Keys in Scale, R1 = Right Side of Piano (+1 Octave)
		bool blackKeyMod = input.getGamepadButton(Input::GamepadButton::LeftShoulder);
		bool rightSideMod = input.getGamepadButton(Input::GamepadButton::RightShoulder);
		updateGamepadStickSelection(state, stickX, stickY, blackKeyMod, rightSideMod);

		// Face Buttons: West (Square) = Loop Scale, East (Circle) = Loop Root Key,
		//               South (Cross) = Play Selected Note, North (Triangle) = Toggle Arp
		if (input.getGamepadButtonDown(Input::GamepadButton::West))
		{
			triggerAction(state, services, State::ACT_SCALE, 1, true);
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::East))
		{
			triggerAction(state, services, State::ACT_ROOT, 1, true);
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::South))
		{
			if (state.selectedStickMidi >= 0)
			{
				playNoteMidi(state.selectedStickMidi, state.selectedInstrument);
			}
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::North))
		{
			state.arpEnabled = !state.arpEnabled;
			state.arpTimer = 0.0f;
			state.arpStep = 0;
			services.audio().playSound({0.02f, state.arpEnabled ? 740.0f : 420.0f, false, vec3(0.0f), 1});
		}

		// Quick Percussion via Stick Clicks (L3 / R3)
		if (input.getGamepadButtonDown(Input::GamepadButton::LeftStick))
		{
			triggerAction(state, services, State::ACT_KICK, 1, false);
		}
		if (input.getGamepadButtonDown(Input::GamepadButton::RightStick))
		{
			triggerAction(state, services, State::ACT_SNARE, 1, false);
		}

		// 5. Quick Percussion Key Bindings
		if (input.getKeyDown(Input::B))
			playNoteMidi(36, Instrument::Kick); // Low kick
		if (input.getKeyDown(Input::N))
			playNoteMidi(38, Instrument::Snare); // Snare
		if (input.getKeyDown(Input::M))
			playNoteMidi(42, Instrument::HiHat); // Hi-Hat
		if (input.getKeyDown(Input::Space))
			playNoteMidi(baseMidi + 0, state.selectedInstrument);

		// 6. Mouse Clicks on On-Screen Piano Keys (black keys take click priority where they overlap white keys)
		bool blackKeyPressed = false;
		for (auto& k : state.keys)
		{
			if (!k.isBlack || !registry.isEntityValid(k.entity) || !registry.hasComponent<ShapeButton>(k.entity))
				continue;

			auto& btn = registry.getComponent<ShapeButton>(k.entity);
			if (btn.state == ButtonState::Down)
			{
				blackKeyPressed = true;
				int playedMidi = k.midiNote + (state.currentOctave - 4) * 12;
				playNoteMidi(playedMidi, state.selectedInstrument);
			}
		}
		if (!blackKeyPressed)
		{
			for (auto& k : state.keys)
			{
				if (k.isBlack || !registry.isEntityValid(k.entity) || !registry.hasComponent<ShapeButton>(k.entity))
					continue;

				auto& btn = registry.getComponent<ShapeButton>(k.entity);
				if (btn.state == ButtonState::Down)
				{
					int playedMidi = k.midiNote + (state.currentOctave - 4) * 12;
					playNoteMidi(playedMidi, state.selectedInstrument);
				}
			}
		}

		// 7. Interactive UI TextButtons (top HUD bar + bottom rows)
		for (size_t i = 0; i < State::NUM_ACTION_BTNS; ++i)
		{
			if (i == State::ACT_MOD && !isModControlEnabled(state, music))
				continue;

			Entity lblEntity = state.actionLabels[i];
			if (registry.isEntityValid(lblEntity) && registry.hasComponent<TextButton>(lblEntity) &&
				registry.getComponent<TextButton>(lblEntity).state == ButtonState::Down)
			{
				triggerAction(state, services, i, 1, true);
			}
		}

		// Sync Arpeggiator TextToggle
		if (registry.isEntityValid(state.arpToggleLabel) && registry.hasComponent<TextToggle>(state.arpToggleLabel))
		{
			auto& tgl = registry.getComponent<TextToggle>(state.arpToggleLabel);
			bool desired = state.prevArpVisual;
			bool userToggled = false;
			if (tgl.active != state.prevArpVisual)
			{
				desired = tgl.active;
				userToggled = true;
			}
			else if (state.arpEnabled != state.prevArpVisual)
			{
				desired = state.arpEnabled;
			}

			tgl.active = desired;
			state.arpEnabled = desired;
			state.prevArpVisual = desired;

			if (userToggled)
			{
				state.arpStep = 0;
				state.arpTimer = 0.0f;
				services.audio().playSound({0.02f, desired ? 740.0f : 420.0f, false, vec3(0.0f), 1});
			}
		}
	}

	void sequencerSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float dt = services.time().deltaTime();

		auto triggerMidi = [&](int midiNote, Instrument inst)
		{ triggerInstrumentMidi(state, services, midiNote, inst); };

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

		// Responsive relayout when window size changes (800x800 desktop vs 640x480 handheld)
		if (Display::width != state.lastWidth || Display::height != state.lastHeight)
		{
			state.lastWidth = Display::width;
			state.lastHeight = Display::height;
			relayoutScene(registry, state, services);
		}

		const InstrumentUiLayout l = computeInstrumentLayout();

		// Decrement key & action flash timers
		for (size_t i = 0; i < 128; ++i)
		{
			if (state.keyFlashTimers[i] > 0.0f)
			{
				state.keyFlashTimers[i] = (std::max)(0.0f, state.keyFlashTimers[i] - dt);
			}
		}
		const auto& music = services.audio().music();
		const Material2DHandle instAccent = scopeAccentMaterial(state, state.selectedInstrument);

		for (size_t i = 0; i < State::NUM_ACTION_BTNS; ++i)
		{
			state.actionFlashTimers[i] = (std::max)(0.0f, state.actionFlashTimers[i] - dt);
			if (registry.isEntityValid(state.actionLabels[i]) &&
				registry.hasComponent<UITextRenderer>(state.actionLabels[i]))
			{
				std::string desiredLbl = buildActionLabel(state, music, i);
				if (desiredLbl != state.cachedActionLabels[i])
				{
					state.cachedActionLabels[i] = desiredLbl;
					auto& tr = registry.getComponent<UITextRenderer>(state.actionLabels[i]);
					tr.text = desiredLbl;
					registry.setComponentDirty(tr);
				}
			}
		}

		// Sync active instrument badge color in Row 0 top bar
		if (registry.isEntityValid(state.actionLabels[State::ACT_INST]) &&
			registry.hasComponent<UITextRenderer>(state.actionLabels[State::ACT_INST]))
		{
			auto& instLbl = registry.getComponent<UITextRenderer>(state.actionLabels[State::ACT_INST]);
			if (registry.hasComponent<TextButton>(state.actionLabels[State::ACT_INST]))
			{
				auto& instBtn = registry.getComponent<TextButton>(state.actionLabels[State::ACT_INST]);
				if (instBtn.baseMaterial != instAccent.id)
				{
					instBtn.baseMaterial = instAccent.id;
					instLbl.material = instAccent.id;
					registry.setComponentDirty(instLbl);
				}
			}
		}

		// Update key downward press displacement, glowing press dot, & scale highlighting
		bool materialChanged = false;
		const bool modEnabled = isModControlEnabled(state, music);
		if (modEnabled != state.cachedModEnabled)
		{
			state.cachedModEnabled = modEnabled;
			if (registry.isEntityValid(state.actionLabels[State::ACT_MOD]) &&
				registry.hasComponent<UITextRenderer>(state.actionLabels[State::ACT_MOD]))
			{
				auto& modLbl = registry.getComponent<UITextRenderer>(state.actionLabels[State::ACT_MOD]);
				uint16_t targetMat = modEnabled ? state.accentAmberMat.id : state.textDimMat.id;
				modLbl.material = targetMat;
				if (registry.hasComponent<TextButton>(state.actionLabels[State::ACT_MOD]))
				{
					auto& modBtn = registry.getComponent<TextButton>(state.actionLabels[State::ACT_MOD]);
					modBtn.baseMaterial = targetMat;
					modBtn.hoverMaterial = modEnabled ? state.textWhiteMat.id : state.textDimMat.id;
				}
				registry.setComponentDirty(modLbl);
			}
		}

		float selDotX = -10000.0f;
		float selDotY = -10000.0f;
		float selDotR = 0.0f;

		for (auto& k : state.keys)
		{
			if (!registry.isEntityValid(k.entity) || !registry.hasComponent<UIShape>(k.entity))
				continue;

			int actualMidi = k.midiNote + (state.currentOctave - 4) * 12;
			float flashTimer = (actualMidi >= 0 && actualMidi < 128) ? state.keyFlashTimers[actualMidi] : 0.0f;
			float pressRatio = std::clamp(flashTimer / 0.25f, 0.0f, 1.0f);

			auto& shape = registry.getComponent<UIShape>(k.entity);

			// Physical downward press effect via position & size parameters (no shader recompile needed!)
			float targetPosY = k.basePosY - (k.isBlack ? 5.0f : 7.0f) * pressRatio;
			float targetSizeX = k.sizeX * (1.0f - 0.06f * pressRatio);
			float targetSizeY = k.sizeY * (1.0f - 0.05f * pressRatio);

			if (std::abs(shape.parameters[DefaultShapes::RoundBox::PosY] - targetPosY) > 0.01f ||
				std::abs(shape.parameters[DefaultShapes::RoundBox::SizeX] - targetSizeX) > 0.01f ||
				std::abs(shape.parameters[DefaultShapes::RoundBox::SizeY] - targetSizeY) > 0.01f)
			{
				shape.parameters[DefaultShapes::RoundBox::PosY] = targetPosY;
				shape.parameters[DefaultShapes::RoundBox::SizeX] = targetSizeX;
				shape.parameters[DefaultShapes::RoundBox::SizeY] = targetSizeY;
				registry.setComponentDirty(shape);
			}

			if (state.selectedStickMidi >= 0 && actualMidi == state.selectedStickMidi)
			{
				selDotX = k.basePosX;
				selDotR = (std::min)(6.0f, targetSizeX * 0.45f);
				selDotY = (l.startY + l.whiteH) + selDotR + 4.0f;
			}

			if (registry.isEntityValid(k.glowDot) && registry.hasComponent<Transform>(k.glowDot))
			{
				auto& dtPos = registry.getComponent<Transform>(k.glowDot).position;
				if (pressRatio > 0.05f)
				{
					dtPos = vec3(k.basePosX, targetPosY - targetSizeY + 12.0f, 0.0f);
				}
				else
				{
					dtPos = vec3(-1000.0f, -1000.0f, 0.0f);
				}
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

		if (registry.isEntityValid(state.selectionDot) && registry.hasComponent<UIShape>(state.selectionDot))
		{
			auto& dotShape = registry.getComponent<UIShape>(state.selectionDot);
			if (std::abs(dotShape.parameters[DefaultShapes::Circle::PosX] - selDotX) > 0.01f ||
				std::abs(dotShape.parameters[DefaultShapes::Circle::PosY] - selDotY) > 0.01f ||
				std::abs(dotShape.parameters[DefaultShapes::Circle::Radius] - selDotR) > 0.01f)
			{
				dotShape.parameters[DefaultShapes::Circle::PosX] = selDotX;
				dotShape.parameters[DefaultShapes::Circle::PosY] = selDotY;
				dotShape.parameters[DefaultShapes::Circle::Radius] = selDotR;
				registry.setComponentDirty(dotShape);
			}
		}

		// Sync ARP toggle label color
		if (registry.isEntityValid(state.arpToggleLabel) && registry.hasComponent<UITextRenderer>(state.arpToggleLabel))
		{
			auto& arpLbl = registry.getComponent<UITextRenderer>(state.arpToggleLabel);
			uint16_t targetMat = state.arpEnabled ? state.accentGreenMat.id : state.textDimMat.id;
			if (arpLbl.material != targetMat)
			{
				arpLbl.material = targetMat;
				registry.setComponentDirty(arpLbl);
			}
		}

		if (materialChanged)
		{
			services.render().forceShaderRefreshUI();
		}

		// Update live oscilloscope dot ribbon (dense overlapping points with tapered amplitude and instrument accent)
		const auto audioData = WeirdAudio::AudioEngine::getInstance().getAudioData();
		const auto& waveform = audioData.waveform;
		const float timeSec = services.time().time();

		float waveformPeak = 0.0f;
		for (float sVal : waveform)
		{
			waveformPeak = (std::max)(waveformPeak, std::abs(sVal));
		}
		const float idleBlend = 1.0f - std::clamp(waveformPeak * 12.0f, 0.0f, 1.0f);

		for (size_t i = 0; i < State::SCOPE_DOTS; ++i)
		{
			if (!registry.isEntityValid(state.scopeDots[i]) || !registry.hasComponent<Transform>(state.scopeDots[i]))
				continue;

			const float frac = static_cast<float>(i) / static_cast<float>(State::SCOPE_DOTS - 1);
			const float x = l.scopeLeftX + (l.scopeRightX - l.scopeLeftX) * frac;
			const float taper = 0.15f + 0.85f * std::sin(frac * 3.14159265f);

			float sample = 0.0f;
			if (!waveform.empty())
			{
				const size_t idx = (i * waveform.size()) / State::SCOPE_DOTS;
				sample = std::clamp(waveform[idx] * 1.45f, -1.0f, 1.0f);
			}
			const float idleWave = 0.08f * std::sin(timeSec * 2.4f + frac * 6.2831853f);
			const float displaced = (sample + idleWave * idleBlend) * taper;

			registry.getComponent<Transform>(state.scopeDots[i]).position =
				vec3(x, l.scopeY + displaced * l.scopeAmp, 0.0f);

			if (registry.hasComponent<UIDot>(state.scopeDots[i]))
			{
				registry.getComponent<UIDot>(state.scopeDots[i]).materialId = instAccent.id;
			}
		}

		// Update last played note readout in top-right corner
		std::string noteStr = formatUiNoteName(state.lastPlayedMidi);
		if (noteStr != state.cachedHudNote && registry.isEntityValid(state.hudNoteText) &&
			registry.hasComponent<UITextRenderer>(state.hudNoteText))
		{
			state.cachedHudNote = noteStr;
			auto& tr = registry.getComponent<UITextRenderer>(state.hudNoteText);
			tr.text = noteStr;
			registry.setComponentDirty(tr);
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

		// Octave & Joystick Deadzone
		ImGui::SliderInt("Octave", &state.currentOctave, 1, 7);
		ImGui::SliderFloat("Joystick Deadzone", &state.stickDeadzone, 0.20f, 0.85f, "%.2f");
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(
				"Radial deadzone for Left/Right note joysticks. Higher values prevent center drift and snapback.");
		}

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

		const bool melodic = InstrumentSceneNamespace::isMelodicInstrument(state.selectedInstrument);
		bool usesPulse = melodic && (activeWave == WaveType::PulseSquare);
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
			ImGui::SetTooltip("Controls duty cycle of Pulse/Square wave (Lead/Bass/Pad). 0.50 = hollow square, 0.10 = "
							  "thin nasal reed.");
		}

		bool usesFM = melodic && (activeWave == WaveType::FMPluck);
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
			ImGui::SetTooltip("Controls metallic bell strike intensity on FM Pluck waveform (Lead/Bass/Pad).");
		}

		bool usesFold = melodic && (activeWave == WaveType::Wavefolder);
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
				"Folds the sine wave back on itself to create rich organ/harmonium overtones (Lead/Bass/Pad).");
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
		{ InstrumentSceneNamespace::triggerInstrumentMidi(state, services, midi, state.selectedInstrument); };

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
			ImGui::BulletText("Left Stick (clockwise from Up): Select Note in Selected Scale (white keys by default)");
			ImGui::BulletText("L1 (Hold): Switch to Black Keys in Scale | R1 (Hold): Right Side of Piano (+1 Octave)");
			ImGui::BulletText("South (Cross / A): Play Selected Note | North (Triangle / Y): Toggle Arp");
			ImGui::BulletText("West (Square / X) / East (Circle / B): Loop Scale Mode / Loop Root Key");
			ImGui::BulletText("L2 / R2 (Triggers): Cycle Instrument (Lead..Hat) / Cycle Wave or Kit");
			ImGui::BulletText("D-Pad Left / Right: Octave Down / Up (O1..O7)");
			ImGui::BulletText("D-Pad Up / Down: Cycle Tone Preset (BRT/PLK/ACD/PAD) / Filter Cutoff");
			ImGui::BulletText("Start: Cycle Mod (PW/FM/Fold) or Preview Drum Hit");
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
