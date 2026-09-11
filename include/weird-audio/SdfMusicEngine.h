#pragma once

#include "weird-audio/SdfSong.h"
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
			uint32_t rngState = 123456789u; // Per-voice fast lock-free XorShift32 RNG
		};

		struct ShapeMusicalParams
		{
			float tempoFactor = 1.0f;	  // [0.7, 1.4] - center distance
			float melodyDensity = 0.5f;	  // [0.0, 1.0] - East distance
			float harmonyRichness = 0.5f; // [0.0, 1.0] - North distance
			float bassWeight = 0.5f;	  // [0.0, 1.0] - South distance
			float percEnergy = 0.5f;	  // [0.0, 1.0] - North-East distance
			float brightness = 0.5f;	  // [0.0, 1.0] - North-West distance
			float syncopation = 0.3f;	  // [0.0, 1.0] - South-East distance
			float variation = 0.5f;		  // [0.0, 1.0] - South-West distance
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

			void setTension(float level);
			float getTension() const
			{
				return m_tension;
			}

			void setEnergy(float level);
			float getEnergy() const
			{
				return m_energy;
			}

			void setHealth(float current, float max);

			// Physics interaction: Ducking & Surge
			void duck(float amount);
			void surge(float amount = 0.5f);
			void resetDynamicEffects();

			// Quantize an arbitrary frequency to the current song's scale
			float quantizeToSongScale(float rawFreq) const;

			// Render music frames into buffer (adds to buffer)
			void render(float* buffer, uint32_t frameCount, uint32_t channels);

			// Play direct note event
			void playNote(float freq, float amp, float durationSec, int instrument = 0, float pan = 0.0f,
						  float filterCutoff = 12000.0f);

			// Re-sample the shape at the 8 compass points (call after modifying shape parameters in real time)
			void resampleShape();
			const ShapeMusicalParams& getShapeParameters() const
			{
				return m_shapeParams;
			}

			const InstrumentRack& getInstrumentRack() const
			{
				return m_rack;
			}

			// Track Isolation & Muting
			void setTrackEnabled(MusicTrack track, bool enabled);
			bool isTrackEnabled(MusicTrack track) const;

			void setTrackToggles(const MusicTrackToggles& toggles)
			{
				m_tracks = toggles;
			}
			const MusicTrackToggles& getTrackToggles() const
			{
				return m_tracks;
			}

			TrackPlayState getTrackPlayState(MusicTrack track) const;
			float getSurgeLevel() const
			{
				return m_surgeLevel;
			}
			float getDuckingLevel() const
			{
				return m_ducking;
			}

			// Motion & Domain Fill Inspection
			float getMotionLevel() const
			{
				return m_motionLevel;
			}
			float getMotionNorm() const
			{
				return m_motionNorm;
			}
			float getFillRatio() const
			{
				return m_fillRatio;
			}
			float getTempoFromMotion() const
			{
				return m_tempoFromMotion;
			}
			float getVolumeFromFill() const
			{
				return m_volumeFromFill;
			}

			// Current playhead position in beats
			float getPlayheadBeat() const
			{
				return m_currentBeat;
			}

			bool isPlaying() const
			{
				return m_playing;
			}
			void setPlaying(bool playing)
			{
				m_playing = playing;
			}

		private:
			uint32_t m_sampleRate = 44100;
			uint32_t m_channels = 2;
			float m_volume = 0.65f;
			bool m_playing = true;

			std::shared_ptr<SdfSong> m_currentSong;
			std::shared_ptr<SdfSong> m_queuedSong;
			std::mutex m_songMutex;

			// Sequencer time tracking
			float m_currentBeat = 0.0f;
			float m_stepAccumulator = 0.0f;
			int m_currentStep = 0; // 16th note step counter
			double m_sceneTime = 0.0;

			// Shape-driven parameter extraction & procedural state
			ShapeMusicalParams m_shapeParams;
			int m_melodyDegree = 0;

			// Dynamic motion & domain fill evaluation
			static constexpr size_t NUM_DOMAIN_SAMPLES = 32;
			float m_motionLevel = 0.0f;
			float m_motionNorm = 0.0f;
			float m_fillRatio = 0.5f;
			float m_tempoFromMotion = 1.0f;
			float m_volumeFromFill = 0.65f;
			size_t m_sampleIndex = 0;
			float m_prevMotionDist = 0.0f;
			bool m_hasPrevMotionSample = false;
			std::array<float, NUM_DOMAIN_SAMPLES> m_domainFillSamples{};
			std::array<float, NUM_DOMAIN_SAMPLES> m_domainMotionSamples{};

			// Dynamic Game Feedback State
			float m_tension = 0.0f;
			float m_energy = 0.5f;
			float m_healthRatio = 1.0f;
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
			float m_surgeLevel = 0.0f;
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

			// Track isolation toggles
			MusicTrackToggles m_tracks;

			void selectInstrumentRack(const ASTFingerprint& fp);
			void sampleShapeParameters();
			void initDomainSamples();
			void sampleDomainMotionAndFill(double sceneTime, double deltaTime = 0.016);
			void on16thStep(int step);
			void evaluateShapeDrivenAtStep(int step);
			bool isTrackDead(MusicTrack track) const;
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
