#pragma once

#include "weird-audio/SdfSong.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		enum class WaveType : uint8_t
		{
			SoftSine = 0,	// 0.85 sin(p) + 0.15 sin(2p) - warm, singing
			BandlimitedSaw, // 4-harmonic additive saw - bright, buzzy string
			PulseSquare,	// Soft-clipped variable pulse - hollow, reedy
			FMPluck,		// 2-op FM with decaying mod index - metallic/bell/pluck
			Wavefolder		// Buchla-style continuous sine fold - rich, evolving
		};

		enum class LeadStyle : uint8_t
		{
			Auto = 0,	   // Chosen automatically from shape & motion
			RepeatingHook, // Catchy 1-bar Call & Response motif
			ChordArpeggio, // Rhythmic chord-tone arpeggiator
			LyricalSinging // Slow, spacious, sustained chord-tone melody
		};

		struct InstrumentRack
		{
			WaveType lead = WaveType::SoftSine;
			WaveType bass = WaveType::SoftSine;
			WaveType pad = WaveType::SoftSine;
			int drumKit = 0; // 0 = 808-style, 1 = punchy acoustic, 2 = industrial/punch
			float pulseWidth = 0.50f;
			float fmModIndex = 2.0f;
			float foldDrive = 2.2f;
		};

		enum class Instrument : uint8_t
		{
			Lead = 0,
			Bass = 1,
			Pad = 4,
			Kick = 5,
			Snare = 6,
			HiHat = 7
		};

		static constexpr std::array<Instrument, 6> SongInstruments = {Instrument::Lead,	 Instrument::Bass,
																	  Instrument::Pad,	 Instrument::Kick,
																	  Instrument::Snare, Instrument::HiHat};

		inline const char* getInstrumentName(Instrument inst)
		{
			switch (inst)
			{
				case Instrument::Lead:
					return "Lead";
				case Instrument::Bass:
					return "Bass";
				case Instrument::Pad:
					return "Pad";
				case Instrument::Kick:
					return "Kick";
				case Instrument::Snare:
					return "Snare";
				case Instrument::HiHat:
					return "Hi-Hat";
				default:
					return "Unknown";
			}
		}

		enum class MusicTrack : uint8_t
		{
			Lead = 0,
			Bass,
			Pad,
			Drums
		};

		struct MusicTrackToggles
		{
			bool lead = true;
			bool bass = true;
			bool pad = true;
			bool drums = true;
		};

		struct MusicTrackVolumes
		{
			float lead = 1.0f;
			float bass = 1.0f;
			float pad = 1.0f;
			float drums = 1.0f;
		};

		enum class TrackPlayState : uint8_t
		{
			Muted = 0, // User manually disabled
			Playing,   // Actively playing
			Paused,	   // Paused by arrangement structure (breakdown/intro/breather)
			Ducked,	   // Suppressed by heavy ducking
			Surged,	   // Overridden & forced active by surge
			Dead	   // Killed sequentially during death sequence
		};

		// Fast, lock-free, branchless XorShift32 noise generator for real-time audio thread safety
		static inline float fastNoise(uint32_t& state)
		{
			if (state == 0)
				state = 123456789u;
			state ^= state << 13;
			state ^= state >> 17;
			state ^= state << 5;
			return (static_cast<float>(state & 0x00FFFFFF) / 8388607.0f) - 1.0f;
		}

		struct MusicVoice
		{
			float frequency = 440.0f;
			float amplitude = 0.0f;
			float decay = 0.3f;
			float time = 0.0f;
			float phase = 0.0f; // Normalized phase in [0.0, 1.0)
			bool finished = false;
			int instrument = 0; // 0=Warm Lead, 1=Warm Bass, 2=Square, 3=Noise, 4=Warm EP/Pad, 5=Kick, 6=Snare, 7=HiHat,
								// 8=UI Click, 9=UI Error
			WaveType waveType = WaveType::SoftSine;
			float waveParam = 0.0f;
			int drumKit = 0;
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 10000.0f;
			float filterQ = 0.7071f;		// Resonance Q (0.7071 = Butterworth flat, >1.0 = resonant peak)
			float filterS1 = 0.0f;			// 2-pole SVF state 1
			float filterS2 = 0.0f;			// 2-pole SVF state 2
			float fadeGain = 1.0f;			// Forced fade-out gain (ducking), linear
			float fadeRate = 0.0f;			// Per-second fadeGain decrease; 0 = no forced fade
			uint32_t rngState = 123456789u; // Per-voice fast lock-free XorShift32 RNG
		};

		struct ShapeVisualTraits
		{
			float mass = 0.5f;		 // [0.0, 1.0] - Area fill & radial reach (how big / heavy)
			float motion = 0.0f;	 // [0.0, 1.0] - Boundary movement speed (how fast it moves)
			float spikiness = 0.0f;	 // [0.0, 1.0] - Perimeter lobes & curvature (smooth vs spiky)
			float hollowness = 0.0f; // [0.0, 1.0] - Center cutout / ring vs solid core
			float asymmetry = 0.0f;	 // [0.0, 1.0] - 180-degree opposite-side imbalance
		};

		struct ShapeMusicalParams
		{
			float tempoFactor = 1.0f;	  // [0.7, 1.4] - Driven by Motion Speed
			float melodyDensity = 0.5f;	  // [0.0, 1.0] - Driven by Spikiness & Motion
			float harmonyRichness = 0.5f; // [0.0, 1.0] - Driven by Size / Mass
			float bassWeight = 0.5f;	  // [0.0, 1.0] - Driven by Size & Core Solidity
			float percEnergy = 0.5f;	  // [0.0, 1.0] - Driven by Motion Speed & Spikiness
			float brightness = 0.5f;	  // [0.0, 1.0] - Driven by Spikiness / Sharpness
			float syncopation = 0.3f;	  // [0.0, 1.0] - Driven by Asymmetry & Hollowness
			float variation = 0.5f;		  // [0.0, 1.0] - Driven by Spikiness & Hollowness
		};

		class SdfMusicEngine
		{
		public:
			SdfMusicEngine();
			~SdfMusicEngine();

			void init(uint32_t sampleRate = 44100, uint32_t channels = 2);

			// Configuration & Active Song
			void setSong(std::shared_ptr<SdfSong> song, bool beatSynced = true);
			std::shared_ptr<SdfSong> getCurrentSong() const;

			// Seamless beat-synced transition (no fade)
			void queueSong(std::shared_ptr<SdfSong> nextSong);

			void setVolume(float volume)
			{
				m_volume = volume;
			}
			float getVolume() const
			{
				return m_volume;
			}

			// Update sequencer timing (called each frame or audio block)
			void update(double deltaTime, double sceneTime);

			// Real-Time Dynamic Game Feedback
			void triggerPositiveFeedback(float intensity = 1.0f);
			void triggerNegativeFeedback(float intensity = 1.0f);
			void triggerDeath();

			// Physics interaction: Ducking & Surge
			void duck(float amount);
			void surge(float amount = 0.5f);
			void resetDynamicEffects();

			// Quantize an arbitrary frequency to the current song's scale
			float quantizeToSongScale(float rawFreq) const;

			// Render music frames into buffer (adds to buffer)
			void render(float* buffer, uint32_t frameCount, uint32_t channels);

			// Play direct note event (filterQ < 0.0f uses instrument default resonance)
			void playNote(float freq, float amp, float durationSec, int instrument = 0, float pan = 0.0f,
						  float filterCutoff = 12000.0f, float filterQ = -1.0f);
			void playNote(float freq, float amp, float durationSec, Instrument instrument, float pan = 0.0f,
						  float filterCutoff = 12000.0f, float filterQ = -1.0f);
			void playNote(int midiNote, float amp = 0.8f, float durationSec = 0.4f,
						  Instrument instrument = Instrument::Lead, float pan = 0.0f, float filterCutoff = 12000.0f,
						  float filterQ = -1.0f);

			static float midiToFrequency(int midiNote)
			{
				return SdfSong::midiToFrequency(midiNote);
			}

			// Re-sample the shape's rotation-invariant visual traits (call after modifying shape parameters)
			void resampleShape();
			ShapeVisualTraits getVisualTraits() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_visualTraits;
			}
			ShapeMusicalParams getShapeParameters() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_shapeParams;
			}
			void setShapeParameters(const ShapeMusicalParams& params)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_shapeParams = params;
			}
			bool isShapeParamsOverridden() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_overrideShapeParams;
			}
			void setShapeParamsOverridden(bool overridden)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_overrideShapeParams = overridden;
				if (!overridden)
				{
					initDomainSamples();
					sampleShapeParameters();
				}
			}
			void setTensionOverride(float tension)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_tensionFromComplexity = std::clamp(tension, 0.0f, 1.0f);
			}
			void resetInstrumentRackToShape()
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				if (m_currentSong)
				{
					selectInstrumentRack(m_currentSong->getFingerprint());
				}
			}

			InstrumentRack getInstrumentRack() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_rack;
			}
			void setInstrumentRack(const InstrumentRack& rack)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_rack = rack;
			}
			void setInstrumentWaveType(Instrument instrument, WaveType waveType);
			WaveType getInstrumentWaveType(Instrument instrument) const;

			void setDrumKit(int kit)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_rack.drumKit = std::clamp(kit, 0, 2);
			}
			int getDrumKit() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_rack.drumKit;
			}

			void setPulseWidth(float pw)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_rack.pulseWidth = std::clamp(pw, 0.05f, 0.50f);
			}
			float getPulseWidth() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_rack.pulseWidth;
			}

			void setFmModIndex(float idx)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_rack.fmModIndex = (std::max)(0.0f, idx);
			}
			float getFmModIndex() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_rack.fmModIndex;
			}

			void setFoldDrive(float drive)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_rack.foldDrive = (std::max)(0.0f, drive);
			}
			float getFoldDrive() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_rack.foldDrive;
			}

			static const char* getWaveTypeName(WaveType waveType);
			static const char* getDrumKitName(int kit);
			static const char* getLeadStyleName(LeadStyle style);

			void setLeadStyle(LeadStyle style)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_leadStyle = style;
			}
			LeadStyle getLeadStyle() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_leadStyle;
			}
			LeadStyle getEffectiveLeadStyle() const;

			// Track Isolation & Muting
			void setTrackEnabled(MusicTrack track, bool enabled);
			bool isTrackEnabled(MusicTrack track) const;

			void setTrackToggles(const MusicTrackToggles& toggles)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_tracks = toggles;
			}
			MusicTrackToggles getTrackToggles() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_tracks;
			}

			void setTrackVolumes(const MusicTrackVolumes& volumes)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_trackVolumes = volumes;
			}
			MusicTrackVolumes getTrackVolumes() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_trackVolumes;
			}

			TrackPlayState getTrackPlayState(MusicTrack track) const;
			float getSurgeLevel() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_surgeLevel;
			}
			float getDuckingLevel() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_ducking;
			}

			// Motion & Domain Fill Inspection
			float getMotionLevel() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_motionLevel;
			}
			float getMotionNorm() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_motionNorm;
			}
			float getFillRatio() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_fillRatio;
			}
			float getTempoFromMotion() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_tempoFromMotion;
			}
			float getVolumeFromFill() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_volumeFromFill;
			}

			// Surface Complexity Inspection (scale-normalized curvature spread of the SDF surface)
			float getComplexityLevel() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_complexityLevel;
			}
			float getComplexityNorm() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_complexityNorm;
			}
			float getComplexitySaturation() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_complexitySaturation;
			}
			// Higher values make the normalized stat cooler (more headroom for complex shapes)
			void setComplexitySaturation(float saturation)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_complexitySaturation = std::max(0.1f, saturation);
			}

			// Tension derived from surface complexity: drives dissonance, brightness and
			// articulation independently of motion (tempo). 0 = calm, 1 = tense.
			float getTension() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_tensionFromComplexity;
			}

			// Tempo & Beat Inspection
			float getTempo() const;
			float getTimeBetweenBeats() const;

			// Current playhead position in beats
			float getPlayheadBeat() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_currentBeat;
			}
			int getLoopCycle() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_loopCycle;
			}

			bool isPlaying() const
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				return m_playing;
			}
			void setPlaying(bool playing)
			{
				std::lock_guard<std::recursive_mutex> lock(m_songMutex);
				m_playing = playing;
			}

		private:
			uint32_t m_sampleRate = 44100;
			uint32_t m_channels = 2;
			float m_volume = 0.65f;
			bool m_playing = true;

			std::shared_ptr<SdfSong> m_currentSong;
			std::shared_ptr<SdfSong> m_queuedSong;
			mutable std::recursive_mutex m_songMutex;

			// Sequencer time tracking
			float m_currentBeat = 0.0f;
			float m_stepAccumulator = 0.0f;
			int m_currentStep = 0; // 16th note step counter (0..511 across 32-bar form)
			int m_loopCycle = 0;   // Increments every 32 bars to evolve melodies & patterns across loops
			double m_sceneTime = 0.0;

			// Shape-driven parameter extraction & procedural state
			ShapeVisualTraits m_visualTraits;
			ShapeMusicalParams m_shapeParams;
			bool m_overrideShapeParams = false;
			LeadStyle m_leadStyle = LeadStyle::Auto;
			int m_melodyDegree = 0;
			int m_melodyLastInterval = 0; // Last scale-degree interval (Narmour gap-fill memory)
			int m_lastLeadStep = -99;
			float m_lastLeadFreq = 0.0f;
			float m_lastLeadVel = 0.0f;
			float m_lastLeadDur = 0.0f;
			float m_lastLeadPan = 0.0f;
			float m_lastLeadCutoff = 2000.0f;

			// Dynamic motion & domain fill evaluation
			static constexpr size_t NUM_DOMAIN_SAMPLES = 32;
			float m_motionLevel = 0.0f;
			float m_motionNorm = 0.0f;
			float m_fillRatio = 0.5f;
			float m_tempoFromMotion = 1.0f;
			float m_volumeFromFill = 0.65f;
			float m_complexityLevel = 0.0f;
			float m_complexityNorm = 0.0f;
			float m_complexitySaturation = 20.0f;
			float m_tensionFromComplexity = 0.0f;
			size_t m_sampleIndex = 0;
			float m_prevMotionDist = 0.0f;
			bool m_hasPrevMotionSample = false;
			std::array<float, NUM_DOMAIN_SAMPLES> m_domainFillSamples{};
			std::array<float, NUM_DOMAIN_SAMPLES> m_domainMotionSamples{};
			std::array<float, NUM_DOMAIN_SAMPLES> m_domainCurvatureSamples{};

			// Dynamic Game Feedback State
			float m_positiveTimer = 0.0f;
			float m_positivePitchOffset = 0.0f;
			float m_negativeTimer = 0.0f;
			float m_concussionFilterCutoff = 20000.0f; // Dips to 280Hz on damage, recovers
			float m_detuneAmount = 0.0f;
			bool m_isDead = false;
			int m_deathStage = 0; // 0=alive, 1=lead dead, 2=drums dead, 3=pad dead, 4=bass dead / all dead
			float m_deathTimer = 0.0f;
			float m_deathSilenceTimer = 0.0f;

			// Ducking & Surge
			float m_ducking = 0.0f;
			float m_duckTarget = 0.0f;
			float m_surgeLevel = 0.0f;
			float m_surgeTarget = 0.0f;
			float m_surgeTimer = 0.0f;
			float m_duckTimer = 0.0f;
			bool m_pendingSurgeImpact = false;
			float m_surgeImpactCooldown = 0.0f;
			std::array<TrackPlayState, 4> m_trackPlayStates{TrackPlayState::Playing, TrackPlayState::Playing,
															TrackPlayState::Playing, TrackPlayState::Playing};

			// Voice Pool
			static constexpr size_t MAX_MUSIC_VOICES = 32;
			std::vector<MusicVoice> m_activeVoices;

			// AST-driven instrument rack
			InstrumentRack m_rack;

			// Track isolation toggles & per-track volume trims
			MusicTrackToggles m_tracks;
			MusicTrackVolumes m_trackVolumes;

			void selectInstrumentRack(const ASTFingerprint& fp);
			void sampleShapeParameters();
			void updateShapeParamsFromTraits();
			void initDomainSamples();
			void sampleDomainMotionAndFill(double sceneTime, double deltaTime = 0.016);
			float estimateSurfaceCurvature(const std::shared_ptr<IMathExpression>& shape, float* params,
										   glm::vec2 samplePoint, float sampleDist);
			void on16thStep(int step);
			void evaluateShapeDrivenAtStep(int step);
			bool isTrackDead(MusicTrack track) const;
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
