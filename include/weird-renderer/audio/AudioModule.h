#pragma once

#include <atomic>
#include <cmath>
#include <functional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// =====================================================================
		// Audio Module System - Modular Procedural Music Generator
		// =====================================================================

		// --- Scale Types ---
		enum class AudioScaleType
		{
			Pentatonic, // Pentatonic scale (e.g. C, D, E, G, A)
			Major,		// Diatonic Major scale (e.g. C, D, E, F, G, A, B)
			Minor,		// Natural Minor scale (e.g. A, B, C, D, E, F, G)
			Chromatic	// Full 12-semitone chromatic scale
		};

		// --- Rhythm Patterns ---
		enum class RhythmPatternId
		{
			Metronome,		 // Steady beat on quarter notes
			OffBeat,		 // Syncopated off-beat rhythm
			FourOnTheFloor,	 // Classic four-on-the-floor dance beat
			HiHat,			 // Fast hi-hat groove
			BassKick,		 // Syncopated bass and kick focused
			AmbientPads,	 // Atmospheric evolving chord pads
			MelodicArpeggio, // Arpeggiated melody line
			JazzSwing,		 // Triplet swung jazz rhythm
			TrapHiHat,		 // Rapid trap hi-hats and 808 sub bass
			ElectronicBass	 // Rolling electronic bass groove
		};

		// --- Instruments / Waveforms ---
		enum class InstrumentType
		{
			Sine,	   // Pure sine wave
			Sawtooth,  // Sawtooth wave
			Square,	   // Square wave
			Noise,	   // White/pink noise
			Polyphonic // Multi-voice blend
		};

		// --- Audio Module Configuration ---
		struct AudioModuleConfig
		{
			std::string sceneName = "default";

			float tempo = 52.0f;		// BPM (relaxed ambient base tempos)
			float beatDuration = 1.15f; // Seconds per beat (60 / BPM)
			AudioScaleType scaleType = AudioScaleType::Pentatonic;
			std::vector<int> scaleRoots = {60}; // MIDI root notes (60 = C4)
			RhythmPatternId rhythmPattern = RhythmPatternId::Metronome;
			InstrumentType instrumentType = InstrumentType::Polyphonic;

			float ambientVolume = 0.35f;	// Chord pads volume
			float melodyVolume = 0.35f;		// Melody layer volume
			float bassVolume = 0.3f;		// Bass layer volume
			float percussionVolume = 0.15f; // Percussion volume
			float baseVolume = 0.65f;		// Master procedural volume

			bool useFrictionModulation = true; // Modulate based on physics friction
			float frictionSensitivity = 0.4f;  // How strongly friction affects tempo/intensity
			float frictionMinTempo = 45.0f;	   // Minimum tempo when calm
			float frictionMaxTempo = 75.0f;	   // Maximum tempo when high friction

			bool randomizeMelody = true;	// Dynamic melody variation
			bool randomizeRhythm = false;	// Dynamic rhythm variation
			float randomizationSeed = 0.0f; // Seed for reproducibility

			float activitySustainSec = 8.0f; // Seconds music sustains before fading when idle
			float activityFadeSpeed = 0.08f; // Fade out rate per second
			bool autoFadeWhenIdle = true;	 // Fade out when quiet, revive on collision

			bool enableProceduralMusic = true;
			bool enableAmbient = true;
		};

		// --- Note Event for Audio Playback ---
		struct NoteEvent
		{
			float frequency = 440.0f;
			float amplitude = 0.5f;
			float duration = 0.3f;
			InstrumentType instrument = InstrumentType::Sine;
			float pan = 0.0f;			   // -1.0f (full left) to +1.0f (full right)
			float filterCutoff = 20000.0f; // Lowpass filter cutoff in Hz
		};

		// --- Audio Module Interface ---
		class AudioModule
		{
		public:
			virtual ~AudioModule() = default;

			// Initialize with configuration
			virtual void initialize(const AudioModuleConfig& config) = 0;

			// Update audio generation (called each frame)
			virtual void update(double time, double deltaTime) = 0;

			// Set friction level (0.0 = calm, 1.0 = heavy friction)
			virtual void setFrictionLevel(float level) = 0;

			// Get current friction level
			virtual float getFrictionLevel() const = 0;

			// Activity level (0.0 = silent/idle, 1.0 = fully active)
			virtual float getActivityLevel() const = 0;
			virtual void setActivityLevel(float level) = 0;

			// Set tempo directly
			virtual void setTempo(float bpm) = 0;

			// Get current tempo
			virtual float getTempo() const = 0;

			// Stop all audio generation
			virtual void stop() = 0;

			// Check if module is active
			virtual bool isActive() const = 0;

			// Dynamic Harmonic Quantization & Collision Integration
			virtual float quantizeToHarmonics(float rawFreq, float impactIntensity) const = 0;
			virtual int getCurrentChordRoot() const = 0;
			virtual void duck(float amount, float durationSec = 0.1f) = 0;
			virtual float getDuckingMultiplier() const = 0;
			virtual void triggerImpact(float intensity, float volume) = 0;
			virtual void surge(float amount = 0.5f) = 0;

			// Get/set configuration
			virtual const AudioModuleConfig& getConfig() const = 0;
			virtual void setConfig(const AudioModuleConfig& config) = 0;
		};

		// --- Procedural Music Generator Implementation ---
		class ProceduralMusicGenerator : public AudioModule
		{
		public:
			ProceduralMusicGenerator();
			~ProceduralMusicGenerator() override = default;

			void initialize(const AudioModuleConfig& config) override;
			void update(double time, double deltaTime) override;
			void setFrictionLevel(float level) override;
			float getFrictionLevel() const override;
			float getActivityLevel() const override;
			void setActivityLevel(float level) override;
			void setTempo(float bpm) override;
			float getTempo() const override;
			void stop() override;
			bool isActive() const override;
			const AudioModuleConfig& getConfig() const override;
			void setConfig(const AudioModuleConfig& config) override;

			// Harmonic Quantization, Ducking & Surge
			float quantizeToHarmonics(float rawFreq, float impactIntensity) const override;
			int getCurrentChordRoot() const override;
			void duck(float amount, float durationSec = 0.1f) override;
			float getDuckingMultiplier() const override;
			void triggerImpact(float intensity, float volume) override;
			void surge(float amount = 0.5f) override;

			void playNote(const NoteEvent& note);

		private:
			AudioModuleConfig m_config;
			bool m_active = true;
			float m_currentTempo = 80.0f;
			float m_frictionLevel = 0.0f;
			float m_smoothedFriction = 0.0f;
			float m_activity = 0.85f; // Dynamic activity (wakes on collision, fades when idle)
			float m_idleTimer = 0.0f;
			float m_ducking = 0.0f;
			double m_stepAccumulator = 0.0;
			int m_step = 0; // 16th note step counter (0..127)

			std::mt19937 m_rng;

			float getScaleFrequency(int rootMidi, int degree, int octaveOffset) const;
			int getChordRoot(int measure) const;
			void on16thStep(int step);
			void playChordPads(int step, int chordRoot);
			void playBass(int step, int chordRoot);
			void playMelody(int step, int chordRoot);
			void playPercussion(int step);
		};

		// --- Empty Audio Module (Silent / Null Object) ---
		class EmptyAudioModule : public AudioModule
		{
		public:
			EmptyAudioModule() = default;
			~EmptyAudioModule() override = default;

			void initialize(const AudioModuleConfig& config) override
			{
				m_config = config;
			}
			void update(double time, double deltaTime) override {}
			void setFrictionLevel(float level) override
			{
				m_frictionLevel = level;
			}
			float getFrictionLevel() const override
			{
				return m_frictionLevel;
			}
			float getActivityLevel() const override
			{
				return 0.0f;
			}
			void setActivityLevel(float level) override {}
			void setTempo(float bpm) override
			{
				m_tempo = bpm;
			}
			float getTempo() const override
			{
				return m_tempo;
			}
			void stop() override
			{
				m_active = false;
			}
			bool isActive() const override
			{
				return false;
			}
			float quantizeToHarmonics(float rawFreq, float impactIntensity) const override
			{
				return rawFreq > 0.0f ? rawFreq : 440.0f;
			}
			int getCurrentChordRoot() const override
			{
				return 60;
			}
			void duck(float amount, float durationSec = 0.1f) override {}
			float getDuckingMultiplier() const override
			{
				return 1.0f;
			}
			void triggerImpact(float intensity, float volume) override {}
			void surge(float amount = 0.5f) override {}
			const AudioModuleConfig& getConfig() const override
			{
				return m_config;
			}
			void setConfig(const AudioModuleConfig& config) override
			{
				m_config = config;
			}

		private:
			AudioModuleConfig m_config;
			float m_frictionLevel = 0.0f;
			float m_tempo = 120.0f;
			bool m_active = false;
		};

		// --- Factory Functions ---
		AudioModule* createProceduralMusicGenerator();
		AudioModule* createEmptyModule();

		// --- Preset Management ---
		void initAudioPresets();
		const AudioModuleConfig* getAudioPreset(const std::string& name);
		std::vector<std::string> listAudioPresets();
		void setAudioModuleFromPreset(AudioModule* module, const std::string& presetName);

	} // namespace WeirdRenderer
} // namespace WeirdEngine
