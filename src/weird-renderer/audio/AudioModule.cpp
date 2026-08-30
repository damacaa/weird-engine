#include "weird-renderer/audio/AudioModule.h"
#include "weird-engine/Logger.h"
#include "weird-renderer/audio/AudioEngine.h"
#include <algorithm>
#include <cmath>
#include <random>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// =====================================================================
		// Procedural Music Generator Implementation
		// =====================================================================

		ProceduralMusicGenerator::ProceduralMusicGenerator()
			: m_rng(1337)
		{
		}

		void ProceduralMusicGenerator::initialize(const AudioModuleConfig& config)
		{
			m_config = config;
			m_currentTempo = config.tempo;
			m_active = config.enableProceduralMusic;
			m_step = 0;
			m_stepAccumulator = 0.0;
			m_frictionLevel = 0.0f;
			m_smoothedFriction = 0.0f;
			m_activity = 0.85f;
			m_idleTimer = 0.0f;

			unsigned int seed = static_cast<unsigned int>(std::hash<std::string>{}(config.sceneName)) +
								static_cast<unsigned int>(config.randomizationSeed * 1000.0f);
			m_rng.seed(seed);
		}

		void ProceduralMusicGenerator::update(double time, double deltaTime)
		{
			if (!m_active || !m_config.enableProceduralMusic)
				return;

			float dt = static_cast<float>(deltaTime);

			// Idle activity decay
			if (m_config.autoFadeWhenIdle)
			{
				m_idleTimer += dt;
				if (m_idleTimer > m_config.activitySustainSec)
				{
					m_activity = (std::max)(0.0f, m_activity - dt * m_config.activityFadeSpeed);
				}
			}

			// Decay ducking
			if (m_ducking > 0.0f)
			{
				m_ducking = (std::max)(0.0f, m_ducking - dt * 8.0f);
			}

			// Smooth friction level
			m_smoothedFriction += (m_frictionLevel - m_smoothedFriction) * (std::min)(1.0f, dt * 5.0f);

			// Calculate dynamic tempo
			float targetTempo = m_config.tempo;
			if (m_config.useFrictionModulation)
			{
				targetTempo = m_config.tempo + (m_config.frictionMaxTempo - m_config.tempo) * m_smoothedFriction *
												   m_config.frictionSensitivity;
			}
			m_currentTempo = targetTempo;

			// 16th note step calculation (4 steps per beat)
			float stepDuration = (60.0f / (std::max)(20.0f, m_currentTempo)) / 4.0f;
			m_stepAccumulator += deltaTime;

			while (m_stepAccumulator >= stepDuration)
			{
				m_stepAccumulator -= stepDuration;
				on16thStep(m_step);
				m_step = (m_step + 1) % 512; // 32-bar form (512 steps ~ 2-3 min multi-section piece)
			}
		}

		void ProceduralMusicGenerator::setFrictionLevel(float level)
		{
			m_frictionLevel = std::clamp(level, 0.0f, 1.0f);
			if (m_frictionLevel > 0.03f)
			{
				m_idleTimer = 0.0f;
				m_activity = (std::min)(1.0f, m_activity + m_frictionLevel * 0.20f);
			}
		}

		float ProceduralMusicGenerator::getFrictionLevel() const
		{
			return m_frictionLevel;
		}

		float ProceduralMusicGenerator::getActivityLevel() const
		{
			return m_activity;
		}

		void ProceduralMusicGenerator::setActivityLevel(float level)
		{
			m_activity = std::clamp(level, 0.0f, 1.0f);
		}

		void ProceduralMusicGenerator::setTempo(float bpm)
		{
			m_config.tempo = bpm;
			m_currentTempo = bpm;
		}

		float ProceduralMusicGenerator::getTempo() const
		{
			return m_currentTempo;
		}

		void ProceduralMusicGenerator::stop()
		{
			m_active = false;
		}

		bool ProceduralMusicGenerator::isActive() const
		{
			return m_active;
		}

		const AudioModuleConfig& ProceduralMusicGenerator::getConfig() const
		{
			return m_config;
		}

		void ProceduralMusicGenerator::setConfig(const AudioModuleConfig& config)
		{
			initialize(config);
		}

		int ProceduralMusicGenerator::getCurrentChordRoot() const
		{
			int measure = (m_step / 16) % 32;
			return getChordRoot(measure);
		}

		void ProceduralMusicGenerator::duck(float amount, float durationSec)
		{
			m_ducking = (std::min)(1.0f, m_ducking + amount);
		}

		float ProceduralMusicGenerator::getDuckingMultiplier() const
		{
			return (std::max)(0.2f, 1.0f - m_ducking * 0.6f);
		}

		void ProceduralMusicGenerator::triggerImpact(float intensity, float volume)
		{
			// Reset idle timer and revive music activity on collision!
			m_idleTimer = 0.0f;
			m_activity = (std::min)(1.0f, m_activity + intensity * 0.45f + volume * 0.35f);

			if (intensity > 0.6f)
			{
				duck(0.4f, 0.1f);
			}
			m_smoothedFriction = (std::min)(1.0f, m_smoothedFriction + intensity * 0.2f);
		}

		void ProceduralMusicGenerator::surge(float amount)
		{
			m_idleTimer = 0.0f;
			m_activity = (std::min)(1.0f, m_activity + amount);
			m_smoothedFriction = (std::min)(1.0f, m_smoothedFriction + amount * 0.4f);

			// High surge (amount >= 0.7f): Celebratory victory flourish synchronized with active chord & scale
			if (amount >= 0.7f && m_config.enableProceduralMusic)
			{
				int chordRoot = getCurrentChordRoot();
				float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
				auto midiToFreq = [](int midi) -> float { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); };

				int thirdOffset = (m_config.scaleType == AudioScaleType::Minor) ? 3 : 4;
				int fifthOffset = 7;
				int octaveOffset = 12;

				float flourishVol = m_config.baseVolume * 0.45f;

				// 1. Root & 5th singing chime (Polyphonic, centered)
				playNote({midiToFreq(chordRoot + octaveOffset), flourishVol * 0.85f, beatSec * 4.5f,
						  InstrumentType::Polyphonic, -0.25f, 4500.0f});
				playNote({midiToFreq(chordRoot + octaveOffset + fifthOffset), flourishVol * 0.80f, beatSec * 4.8f,
						  InstrumentType::Polyphonic, 0.30f, 5200.0f});

				// 2. High sparkle chime (Sine, wide stereo)
				playNote({midiToFreq(chordRoot + octaveOffset * 2 + thirdOffset), flourishVol * 0.70f, beatSec * 5.2f,
						  InstrumentType::Sine, -0.65f, 6500.0f});
				playNote({midiToFreq(chordRoot + octaveOffset * 2 + 12), flourishVol * 0.65f, beatSec * 5.8f,
						  InstrumentType::Sine, 0.70f, 7500.0f});

				// 3. Deep warm sub root resolution
				playNote({midiToFreq(chordRoot - 24), flourishVol * 0.75f, beatSec * 4.0f, InstrumentType::Sine, 0.0f,
						  450.0f});
			}
		}

		float ProceduralMusicGenerator::quantizeToHarmonics(float rawFreq, float impactIntensity) const
		{
			int chordRoot = getCurrentChordRoot();

			// If raw frequency is provided (> 20Hz), snap to the nearest scale note in the active chord
			if (rawFreq > 20.0f)
			{
				float midiNote = 69.0f + 12.0f * std::log2(rawFreq / 440.0f);
				int roundedMidi = static_cast<int>(std::round(midiNote));

				static const int pentatonicDegrees[5] = {0, 2, 4, 7, 9};
				static const int majorDegrees[7] = {0, 2, 4, 5, 7, 9, 11};
				static const int minorDegrees[7] = {0, 2, 3, 5, 7, 8, 10};

				const int* degrees = pentatonicDegrees;
				int numDegrees = 5;
				if (m_config.scaleType == AudioScaleType::Major)
				{
					degrees = majorDegrees;
					numDegrees = 7;
				}
				else if (m_config.scaleType == AudioScaleType::Minor)
				{
					degrees = minorDegrees;
					numDegrees = 7;
				}

				int octave = (roundedMidi - chordRoot) / 12;
				int semitoneInOctave = ((roundedMidi - chordRoot) % 12 + 12) % 12;

				int closestDegree = degrees[0];
				int minDistance = 100;
				for (int i = 0; i < numDegrees; ++i)
				{
					int dist = std::abs(degrees[i] - semitoneInOctave);
					if (dist < minDistance)
					{
						minDistance = dist;
						closestDegree = degrees[i];
					}
				}

				int finalMidi = chordRoot + octave * 12 + closestDegree;
				return 440.0f * std::pow(2.0f, (finalMidi - 69.0f) / 12.0f);
			}

			// If rawFreq <= 0.0f (auto-generate from impact intensity):
			// Low intensity (< 0.35) -> high sparkle (7th/9th interval, octave 5-6)
			// Medium intensity (0.35..0.7) -> melodic triad (3rd/5th, octave 4)
			// Heavy intensity (>= 0.7) -> deep bass root (octave 2)
			int targetMidi = chordRoot;
			if (impactIntensity < 0.35f)
			{
				int degree = (m_config.scaleType == AudioScaleType::Minor) ? 10 : 9;
				targetMidi = chordRoot + 24 + degree;
			}
			else if (impactIntensity < 0.70f)
			{
				int thirdOffset = (m_config.scaleType == AudioScaleType::Minor) ? 3 : 4;
				targetMidi = chordRoot + 12 + thirdOffset;
			}
			else
			{
				targetMidi = chordRoot - 24;
			}

			return 440.0f * std::pow(2.0f, (targetMidi - 69.0f) / 12.0f);
		}

		void ProceduralMusicGenerator::playNote(const NoteEvent& note)
		{
			float amp = note.amplitude * m_config.baseVolume;
			if (amp <= 0.001f)
				return;

			// Constant-power stereo panning law: [-1, +1] -> left/right gains
			float panNorm = std::clamp((note.pan + 1.0f) * 0.5f, 0.0f, 1.0f);
			float leftGain = std::cos(panNorm * 1.5707963f);
			float rightGain = std::sin(panNorm * 1.5707963f);

			AudioEngine::getInstance().playVoice(note.frequency, amp, note.duration, note.instrument, leftGain,
												 rightGain, note.filterCutoff);
		}

		// =====================================================================
		// Harmony & Scale Helpers
		// =====================================================================

		float ProceduralMusicGenerator::getScaleFrequency(int rootMidi, int degree, int octaveOffset) const
		{
			static const std::vector<int> pentatonicIntervals = {0, 2, 4, 7, 9};
			static const std::vector<int> majorIntervals = {0, 2, 4, 5, 7, 9, 11};
			static const std::vector<int> minorIntervals = {0, 2, 3, 5, 7, 8, 10};
			static const std::vector<int> chromaticIntervals = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

			const std::vector<int>* intervals = &pentatonicIntervals;
			switch (m_config.scaleType)
			{
				case AudioScaleType::Major:
					intervals = &majorIntervals;
					break;
				case AudioScaleType::Minor:
					intervals = &minorIntervals;
					break;
				case AudioScaleType::Chromatic:
					intervals = &chromaticIntervals;
					break;
				case AudioScaleType::Pentatonic:
				default:
					intervals = &pentatonicIntervals;
					break;
			}

			int numNotes = static_cast<int>(intervals->size());
			int oct = degree / numNotes;
			int idx = degree % numNotes;
			if (idx < 0)
			{
				idx += numNotes;
				oct -= 1;
			}

			int noteMidi = rootMidi + (octaveOffset + oct) * 12 + (*intervals)[idx];
			return 440.0f * std::pow(2.0f, (noteMidi - 69) / 12.0f);
		}

		int ProceduralMusicGenerator::getChordRoot(int measure) const
		{
			int root = m_config.scaleRoots.empty() ? 60 : m_config.scaleRoots[0];
			measure = measure % 32;

			switch (m_config.scaleType)
			{
				case AudioScaleType::Minor:
					// 32-bar form across 4 distinct 8-bar sections
					switch (measure)
					{
						// Part A: Exposition
						case 0:
							return root;
						case 1:
							return root + 8;
						case 2:
							return root + 3;
						case 3:
							return root + 10;
						case 4:
							return root;
						case 5:
							return root + 5;
						case 6:
							return root + 7;
						case 7:
							return root;

						// Part B: Modulation & Lift
						case 8:
							return root + 8;
						case 9:
							return root + 10;
						case 10:
							return root;
						case 11:
							return root + 7;
						case 12:
							return root + 5;
						case 13:
							return root + 8;
						case 14:
							return root + 10;
						case 15:
							return root;

						// Part C: Spacious Ambient Bridge
						case 16:
							return root + 3;
						case 17:
							return root + 10;
						case 18:
							return root + 8;
						case 19:
							return root + 7;
						case 20:
							return root + 5;
						case 21:
							return root + 7;
						case 22:
							return root;
						case 23:
							return root;

						// Part D: Climax & Resolution
						case 24:
							return root + 5;
						case 25:
							return root + 8;
						case 26:
							return root + 10;
						case 27:
							return root;
						case 28:
							return root + 5;
						case 29:
							return root + 7;
						case 30:
							return root + 10;
						case 31:
						default:
							return root;
					}
					break;

				case AudioScaleType::Major:
					switch (measure)
					{
						// Part A: Main Theme
						case 0:
							return root;
						case 1:
							return root + 9;
						case 2:
							return root + 5;
						case 3:
							return root + 7;
						case 4:
							return root;
						case 5:
							return root + 5;
						case 6:
							return root + 9;
						case 7:
							return root + 7;

						// Part B: Turnaround & Color
						case 8:
							return root + 9;
						case 9:
							return root + 2;
						case 10:
							return root + 5;
						case 11:
							return root + 7;
						case 12:
							return root + 4;
						case 13:
							return root + 9;
						case 14:
							return root + 2;
						case 15:
							return root + 7;

						// Part C: Open Spacious Bridge
						case 16:
							return root + 5;
						case 17:
							return root;
						case 18:
							return root + 9;
						case 19:
							return root + 7;
						case 20:
							return root + 5;
						case 21:
							return root + 7;
						case 22:
							return root;
						case 23:
							return root;

						// Part D: Full Cadence
						case 24:
							return root + 2;
						case 25:
							return root + 5;
						case 26:
							return root + 7;
						case 27:
							return root;
						case 28:
							return root + 9;
						case 29:
							return root + 5;
						case 30:
							return root + 7;
						case 31:
						default:
							return root;
					}
					break;

				case AudioScaleType::Chromatic:
					switch (measure % 8)
					{
						case 0:
							return root;
						case 1:
							return root + 3;
						case 2:
							return root + 6;
						case 3:
							return root + 10;
						case 4:
							return root + 1;
						case 5:
							return root + 5;
						case 6:
							return root + 8;
						case 7:
						default:
							return root + 11;
					}
					break;

				case AudioScaleType::Pentatonic:
				default:
					switch (measure)
					{
						// Part A: Uplifting Zen Flow
						case 0:
							return root;
						case 1:
							return root + 5;
						case 2:
							return root + 9;
						case 3:
							return root + 7;
						case 4:
							return root;
						case 5:
							return root + 2;
						case 6:
							return root + 5;
						case 7:
							return root + 7;

						// Part B: Melodic Departure
						case 8:
							return root + 9;
						case 9:
							return root + 5;
						case 10:
							return root;
						case 11:
							return root + 7;
						case 12:
							return root + 2;
						case 13:
							return root + 9;
						case 14:
							return root + 5;
						case 15:
							return root + 7;

						// Part C: Floating Ambient Bridge
						case 16:
							return root + 5;
						case 17:
							return root;
						case 18:
							return root + 9;
						case 19:
							return root + 7;
						case 20:
							return root + 5;
						case 21:
							return root + 7;
						case 22:
							return root;
						case 23:
							return root;

						// Part D: Celebratory Resolution
						case 24:
							return root;
						case 25:
							return root + 2;
						case 26:
							return root + 5;
						case 27:
							return root + 9;
						case 28:
							return root + 2;
						case 29:
							return root + 5;
						case 30:
							return root + 7;
						case 31:
						default:
							return root;
					}
					break;
			}
			return root;
		}

		// =====================================================================
		// Step Sequencer Logic
		// =====================================================================

		void ProceduralMusicGenerator::on16thStep(int step)
		{
			if (m_activity <= 0.01f)
				return; // Complete silence when idle

			int measure = (step / 16) % 32;
			int subStep = step % 16;

			// Global phrase breath: at the end of each 8-measure section (specifically last 4 steps),
			// pause all new note strikes for a moment of quiet stillness
			if ((measure % 8 == 7) && subStep >= 12)
			{
				return;
			}

			int chordRoot = getChordRoot(measure);

			playChordPads(step, chordRoot);
			playBass(step, chordRoot);
			playMelody(step, chordRoot);
			playPercussion(step);
		}

		void ProceduralMusicGenerator::playChordPads(int step, int chordRoot)
		{
			if (m_config.ambientVolume <= 0.01f || m_activity <= 0.02f)
				return;

			int subStep = step % 16;
			int measure = (step / 16) % 32;

			// Trigger main chord pad at start of measure, and subtle airy re-swell on step 8 in Part C (Bridge)
			bool triggerPad = (subStep == 0) || (subStep == 8 && (measure >= 16 && measure < 24));
			if (triggerPad)
			{
				float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
				float padDuration = beatSec * (subStep == 0 ? 6.5f : 3.5f); // Long lush ambient sustain
				float activityScale = (std::min)(1.0f, m_activity * 1.7f);
				float padVol = m_config.ambientVolume * 0.34f * getDuckingMultiplier() * activityScale;

				int thirdOffset = (m_config.scaleType == AudioScaleType::Minor) ? 3 : 4;
				int fifthOffset = 7;
				int seventhOffset = (m_config.scaleType == AudioScaleType::Minor) ? 10 : 11;
				int ninthOffset = 14;

				auto midiToFreq = [](int midi) -> float { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); };

				// 1. Root Note (Center-Left, warm low-pass)
				playNote({midiToFreq(chordRoot), padVol * 0.9f, padDuration, InstrumentType::Sine, -0.25f, 2500.0f});

				// 2. Fifth (Center-Right, open harmony)
				playNote({midiToFreq(chordRoot + fifthOffset), padVol * 0.8f, padDuration, InstrumentType::Sine, 0.35f,
						  3800.0f});

				// 3. Third or Suspended 2nd/4th (Wide Left)
				int colorTone = thirdOffset;
				if (measure % 4 == 2 && subStep == 0)
				{
					colorTone = 2; // sus2 flavor
				}
				else if (measure % 4 == 3 && subStep == 0)
				{
					colorTone = 5; // sus4 flavor
				}
				playNote({midiToFreq(chordRoot + colorTone), padVol * 0.70f, padDuration, InstrumentType::Sine, -0.65f,
						  4500.0f});

				// 4. Seventh or Ninth shimmer (Wide Right, upper air)
				int shimmerTone = (measure % 2 == 0) ? seventhOffset : ninthOffset;
				playNote({midiToFreq(chordRoot + shimmerTone), padVol * 0.55f, padDuration, InstrumentType::Sine, 0.75f,
						  5500.0f});

				// 5. Ambient stereo detune sparkle for Polyphonic instrument
				if (m_config.instrumentType == InstrumentType::Polyphonic)
				{
					playNote({midiToFreq(chordRoot + 12) * 1.002f, padVol * 0.32f, padDuration * 0.85f,
							  InstrumentType::Polyphonic, 0.5f, 4200.0f});
					playNote({midiToFreq(chordRoot + 12) * 0.998f, padVol * 0.32f, padDuration * 0.85f,
							  InstrumentType::Polyphonic, -0.5f, 4200.0f});
				}
			}
		}

		void ProceduralMusicGenerator::playBass(int step, int chordRoot)
		{
			if (m_config.bassVolume <= 0.01f || m_activity <= 0.15f)
				return;

			int subStep = step % 16;
			int measure = (step / 16) % 32;

			// --- Instrument Pause / Arrangement Breathing ---
			// Bass pauses during measures 3, 6..7, 11, 14..23 (floating ambient sections)
			if (measure == 3 || (measure >= 6 && measure <= 7) || measure == 11 || (measure >= 14 && measure <= 23))
			{
				// In measure 16 (start of bridge), trigger one single deep sub drone then stay silent
				if (measure == 16 && subStep == 0)
				{
					float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
					int bassMidi = chordRoot - 24;
					auto midiToFreq = [](int midi) -> float { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); };
					playNote({midiToFreq(bassMidi), m_config.bassVolume * 0.45f, beatSec * 6.0f, InstrumentType::Sine,
							  0.0f, 500.0f});
				}
				return;
			}

			float activityScale = (m_activity - 0.15f) / 0.85f;
			int bassMidi = chordRoot - 24; // 2 octaves down
			float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
			float bassDur = beatSec * 1.8f; // Longer sustained notes
			float bassVol = m_config.bassVolume * 0.50f * activityScale;

			InstrumentType bassInst = InstrumentType::Sine;
			if (m_config.instrumentType == InstrumentType::Sawtooth)
				bassInst = InstrumentType::Sawtooth;
			else if (m_config.instrumentType == InstrumentType::Square)
				bassInst = InstrumentType::Square;

			auto midiToFreq = [](int midi) -> float { return 440.0f * std::pow(2.0f, (midi - 69) / 12.0f); };

			bool play = false;
			int pitchOffset = 0;

			// Spacious slow bass: only play on downbeat (step 0) or occasionally step 8 (half-bar spacing)
			if (subStep == 0)
			{
				play = true;
			}
			else if (subStep == 8 && (measure % 2 == 1))
			{
				play = true;
				pitchOffset = 7; // Fifth
			}

			if (play)
			{
				playNote({midiToFreq(bassMidi + pitchOffset), bassVol, bassDur, bassInst, 0.0f, 650.0f});
			}
		}

		void ProceduralMusicGenerator::playMelody(int step, int chordRoot)
		{
			if (m_config.melodyVolume <= 0.01f || m_activity <= 0.22f)
				return;

			int subStep = step % 16;
			int measure = (step / 16) % 32;

			// --- Instrument Pause / Arrangement Breathing ---
			// Melody pauses for entire measures to let the ambient space breathe:
			// - Measures 3..5: 3-measure pause
			// - Measures 11..15: 5-measure pause
			// - Measures 18..23: 6-measure ambient stillness
			// - Measures 28..31: 4-measure sparse outro
			if ((measure >= 3 && measure <= 5) || (measure >= 11 && measure <= 15) ||
				(measure >= 18 && measure <= 23) || (measure >= 28 && measure <= 31))
			{
				// Occasional single bell chime in the silence
				if (subStep == 8 && (measure == 4 || measure == 12 || measure == 20))
				{
					float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
					int leadRoot = chordRoot + 12;
					float chimeFreq = getScaleFrequency(leadRoot, 4, 1);
					playNote({chimeFreq, m_config.melodyVolume * 0.30f, beatSec * 3.5f, InstrumentType::Polyphonic,
							  0.65f, 4500.0f});
				}
				return;
			}

			// Small pause at the end of each active measure (step 12..15): 1-beat silence before next bar
			if (subStep >= 12)
			{
				return;
			}

			float activityScale = (m_activity - 0.22f) / 0.78f;
			float melVol = m_config.melodyVolume * 0.42f * activityScale;
			int leadRoot = chordRoot + 12; // Octave up
			float beatSec = 60.0f / (std::max)(20.0f, m_currentTempo);
			InstrumentType leadInst = m_config.instrumentType;

			// Spacious, longer singing melody notes (triggering only 1 to 2 times per measure)
			bool shouldPlay = false;
			int deg = 0;
			int octOffset = 0;
			float noteDur = beatSec * 2.8f; // Sits and rings out for 2.8 beats

			if (m_config.rhythmPattern == RhythmPatternId::MelodicArpeggio)
			{
				static const int calmDegrees[8] = {0, 4, 7, 9, 7, 4, 2, 0};
				int noteIdx = (measure % 4) * 2 + (subStep == 0 ? 0 : 1);
				if (subStep == 0)
				{
					shouldPlay = true;
					deg = calmDegrees[noteIdx % 8];
					noteDur = beatSec * 3.0f;
				}
				else if (subStep == 6 && (measure % 2 == 1))
				{
					shouldPlay = true;
					deg = calmDegrees[(noteIdx + 1) % 8];
					octOffset = (measure % 4 == 3) ? 1 : 0;
					noteDur = beatSec * 2.2f;
				}
			}
			else if (m_config.randomizeMelody)
			{
				if (subStep == 0)
				{
					shouldPlay = true;
					static const int choices[5] = {0, 2, 4, 7, 9};
					deg = choices[m_rng() % 5];
					octOffset = (m_rng() % 5 == 0) ? 1 : 0;
					noteDur = beatSec * 3.2f;
				}
				else if (subStep == 6 && (m_rng() % 2 == 0))
				{
					shouldPlay = true;
					static const int choices[5] = {2, 4, 7, 9, 11};
					deg = choices[m_rng() % 5];
					noteDur = beatSec * 2.2f;
				}
			}
			else
			{
				// Thematic long bell tones
				if (subStep == 0)
				{
					shouldPlay = true;
					static const int themeDegrees[4] = {0, 4, 2, 7};
					deg = themeDegrees[measure % 4];
					noteDur = beatSec * 3.2f;
				}
				else if (subStep == 8 && (measure % 2 == 0))
				{
					shouldPlay = true;
					deg = 9;
					noteDur = beatSec * 2.0f;
				}
			}

			if (shouldPlay)
			{
				float freq = getScaleFrequency(leadRoot, deg, octOffset);
				float notePan = std::sin(step * 0.35f + deg * 0.6f) * 0.70f;
				float cutoff = 3500.0f + 2000.0f * (1.0f - (deg / 10.0f));

				playNote({freq, melVol, noteDur, leadInst, notePan, cutoff});

				// Subtle stereophonic echo on strong downbeats
				if (subStep == 0 && m_activity > 0.45f)
				{
					playNote({freq, melVol * 0.25f, noteDur * 1.2f, InstrumentType::Sine, -notePan, cutoff * 0.7f});
				}
			}

			// Atmospheric zen wind chime / sparkle (every few bars)
			if (subStep == 6 && (measure % 4 == 2))
			{
				if (m_activity > 0.28f)
				{
					int chimeDeg = (m_rng() % 4) * 2 + 2;
					float chimeFreq = getScaleFrequency(leadRoot, chimeDeg, 2); // 2 octaves up
					float chimePan = (m_rng() % 2 == 0) ? -0.80f : 0.80f;
					playNote({chimeFreq, melVol * 0.32f, beatSec * 3.8f, InstrumentType::Sine, chimePan, 7000.0f});
				}
			}
		}

		void ProceduralMusicGenerator::playPercussion(int step)
		{
			if (m_config.percussionVolume <= 0.01f || m_activity <= 0.45f)
				return;

			int subStep = step % 16;
			int measure = (step / 16) % 32;

			// --- Instrument Pause / Arrangement Breathing ---
			// Percussion is muted in most measures: only plays sparsely on measures 0..2 and 24..26
			if (!((measure >= 0 && measure <= 2) || (measure >= 24 && measure <= 26)))
			{
				return;
			}

			// Small pause at measure tail
			if (subStep >= 12)
			{
				return;
			}

			float activityScale = (m_activity - 0.45f) / 0.55f;
			float pVol = m_config.percussionVolume * 0.40f * activityScale;

			auto playKick = [&](float vol)
			{ playNote({50.0f, vol * 0.8f, 0.15f, InstrumentType::Sine, 0.0f, 200.0f}); };

			auto playClosedHat = [&](float vol, float pan = 0.35f)
			{ playNote({1200.0f, vol * 0.28f, 0.04f, InstrumentType::Noise, pan, 6500.0f}); };

			// Minimal, gentle acoustic brush ticks
			if (subStep == 0)
			{
				playKick(pVol * 0.6f);
			}
			else if (subStep == 8)
			{
				playClosedHat(pVol * 0.5f, 0.35f);
			}
		}

		// =====================================================================
		// Factory Functions
		// =====================================================================

		AudioModule* createProceduralMusicGenerator()
		{
			return new ProceduralMusicGenerator();
		}

		AudioModule* createEmptyModule()
		{
			return new EmptyAudioModule();
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
