#pragma once

#include "weird-renderer/audio/SdfSong.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
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

		struct MusicVoice
		{
			float frequency = 440.0f;
			float amplitude = 0.0f;
			float decay = 0.3f;
			float time = 0.0f;
			float phase = 0.0f;
			bool finished = false;
			int instrument = 0; // 0=Warm Lead, 1=Warm Bass, 2=Square, 3=Noise, 4=Warm EP/Pad, 5=Kick, 6=Snare, 7=HiHat
			WaveType waveType = WaveType::SoftSine;
			float waveParam = 0.0f;
			int drumKit = 0;
			float leftGain = 0.7071f;
			float rightGain = 0.7071f;
			float filterCutoff = 10000.0f;
			float filterState = 0.0f;
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
			float m_motionLevel = 0.0f;
			float m_motionNorm = 0.0f;
			float m_fillRatio = 0.5f;
			float m_tempoFromMotion = 1.0f;
			float m_volumeFromFill = 0.65f;

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
			float m_deathSlowdown = 1.0f;

			// Ducking
			float m_ducking = 0.0f;

			// Voice Pool
			static constexpr size_t MAX_MUSIC_VOICES = 32;
			std::vector<MusicVoice> m_activeVoices;

			// AST-driven instrument rack
			InstrumentRack m_rack;

			// Track isolation toggles
			MusicTrackToggles m_tracks;

			void selectInstrumentRack(const ASTFingerprint& fp);
			void sampleShapeParameters();
			void sampleDomainMotionAndFill(double sceneTime);
			void on16thStep(int step);
			void evaluateShapeDrivenAtStep(int step);
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine
