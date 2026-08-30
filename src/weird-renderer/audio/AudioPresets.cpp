// =====================================================================
// Audio Presets Implementation - Static Configuration Instances
// Each preset defines a unique "song" for its scene through code
// =====================================================================

#include "weird-renderer/audio/AudioPresets.h"
#include <unordered_map>

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// --- Aquarium Scene: Calming underwater ambience with gentle bubbles ---
		static const AudioModuleConfig g_aquariumConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "aquarium";
			config.tempo = 48.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {57}; // A minor
			config.rhythmPattern = RhythmPatternId::AmbientPads;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.50f;
			config.melodyVolume = 0.25f;
			config.bassVolume = 0.15f;
			config.percussionVolume = 0.05f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = false;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 10.0f;
			config.activityFadeSpeed = 0.06f;
			return config;
		}();

		const AudioModuleConfig& getAquariumConfig()
		{
			return g_aquariumConfig;
		}

		// --- Service Showcase: Dynamic physics demo with energetic beats ---
		static const AudioModuleConfig g_showcaseConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "showcase";
			config.tempo = 94.0f;
			config.scaleType = AudioScaleType::Major;
			config.scaleRoots = {60}; // C major
			config.rhythmPattern = RhythmPatternId::FourOnTheFloor;
			config.instrumentType = InstrumentType::Sawtooth;
			config.ambientVolume = 0.20f;
			config.melodyVolume = 0.45f;
			config.bassVolume = 0.4f;
			config.percussionVolume = 0.35f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.6f;
			config.frictionMinTempo = 80.0f;
			config.frictionMaxTempo = 125.0f;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getShowcaseConfig()
		{
			return g_showcaseConfig;
		}

		// --- Shapes Combinations: Creative exploration with playful melodies ---
		static const AudioModuleConfig g_shapesConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "shapes";
			config.tempo = 72.0f;
			config.scaleType = AudioScaleType::Pentatonic;
			config.scaleRoots = {67}; // G major pentatonic
			config.rhythmPattern = RhythmPatternId::MelodicArpeggio;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.25f;
			config.melodyVolume = 0.5f;
			config.bassVolume = 0.25f;
			config.percussionVolume = 0.15f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.4f;
			config.frictionMinTempo = 60.0f;
			config.frictionMaxTempo = 105.0f;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 9.0f;
			config.activityFadeSpeed = 0.07f;
			return config;
		}();

		const AudioModuleConfig& getShapesConfig()
		{
			return g_shapesConfig;
		}

		// --- Rope Scene: Tension and release with rhythmic patterns ---
		static const AudioModuleConfig g_ropeConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "rope";
			config.tempo = 76.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {57}; // A minor
			config.rhythmPattern = RhythmPatternId::BassKick;
			config.instrumentType = InstrumentType::Sawtooth;
			config.ambientVolume = 0.20f;
			config.melodyVolume = 0.35f;
			config.bassVolume = 0.45f;
			config.percussionVolume = 0.30f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.5f;
			config.frictionMinTempo = 65.0f;
			config.frictionMaxTempo = 110.0f;
			config.randomizeMelody = false;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getRopeConfig()
		{
			return g_ropeConfig;
		}

		// --- Life Scene: Organic growth with natural rhythms ---
		static const AudioModuleConfig g_lifeConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "life";
			config.tempo = 56.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {52}; // E minor
			config.rhythmPattern = RhythmPatternId::OffBeat;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.40f;
			config.melodyVolume = 0.3f;
			config.bassVolume = 0.25f;
			config.percussionVolume = 0.12f;
			config.baseVolume = 0.55f;
			config.useFrictionModulation = false;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 10.0f;
			config.activityFadeSpeed = 0.06f;
			return config;
		}();

		const AudioModuleConfig& getLifeConfig()
		{
			return g_lifeConfig;
		}

		// --- Walk Scene: Journey and movement with steady rhythm ---
		static const AudioModuleConfig g_walkConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "walk";
			config.tempo = 66.0f;
			config.scaleType = AudioScaleType::Major;
			config.scaleRoots = {62}; // D major
			config.rhythmPattern = RhythmPatternId::Metronome;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.25f;
			config.melodyVolume = 0.45f;
			config.bassVolume = 0.3f;
			config.percussionVolume = 0.20f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.3f;
			config.frictionMinTempo = 55.0f;
			config.frictionMaxTempo = 95.0f;
			config.randomizeMelody = false;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getWalkConfig()
		{
			return g_walkConfig;
		}

		// --- Text Scene: Narrative flow with melodic storytelling ---
		static const AudioModuleConfig g_textConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "text";
			config.tempo = 58.0f;
			config.scaleType = AudioScaleType::Pentatonic;
			config.scaleRoots = {65}; // F major pentatonic
			config.rhythmPattern = RhythmPatternId::MelodicArpeggio;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.35f;
			config.melodyVolume = 0.50f;
			config.bassVolume = 0.2f;
			config.percussionVolume = 0.06f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = false;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 10.0f;
			config.activityFadeSpeed = 0.06f;
			return config;
		}();

		const AudioModuleConfig& getTextConfig()
		{
			return g_textConfig;
		}

		// --- Image Scene: Visual exploration with atmospheric pads ---
		static const AudioModuleConfig g_imageConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "image";
			config.tempo = 40.0f;
			config.scaleType = AudioScaleType::Chromatic;
			config.scaleRoots = {60}; // C chromatic
			config.rhythmPattern = RhythmPatternId::AmbientPads;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.55f;
			config.melodyVolume = 0.2f;
			config.bassVolume = 0.15f;
			config.percussionVolume = 0.04f;
			config.baseVolume = 0.55f;
			config.useFrictionModulation = false;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 12.0f;
			config.activityFadeSpeed = 0.05f;
			return config;
		}();

		const AudioModuleConfig& getImageConfig()
		{
			return g_imageConfig;
		}

		// --- Destroy Scene: Intense action with driving rhythm ---
		static const AudioModuleConfig g_destroyConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "destroy";
			config.tempo = 98.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {62}; // D minor
			config.rhythmPattern = RhythmPatternId::TrapHiHat;
			config.instrumentType = InstrumentType::Square;
			config.ambientVolume = 0.15f;
			config.melodyVolume = 0.3f;
			config.bassVolume = 0.50f;
			config.percussionVolume = 0.45f;
			config.baseVolume = 0.65f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.8f;
			config.frictionMinTempo = 85.0f;
			config.frictionMaxTempo = 135.0f;
			config.randomizeMelody = false;
			config.randomizeRhythm = false;
			config.activitySustainSec = 7.0f;
			config.activityFadeSpeed = 0.09f;
			return config;
		}();

		const AudioModuleConfig& getDestroyConfig()
		{
			return g_destroyConfig;
		}

		// --- Mouse Collision Scene: Interactive feedback with responsive beats ---
		static const AudioModuleConfig g_mouseCollisionConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "mouse_collision";
			config.tempo = 82.0f;
			config.scaleType = AudioScaleType::Major;
			config.scaleRoots = {60}; // C major
			config.rhythmPattern = RhythmPatternId::HiHat;
			config.instrumentType = InstrumentType::Square;
			config.ambientVolume = 0.20f;
			config.melodyVolume = 0.4f;
			config.bassVolume = 0.35f;
			config.percussionVolume = 0.35f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.7f;
			config.frictionMinTempo = 70.0f;
			config.frictionMaxTempo = 115.0f;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getMouseCollisionConfig()
		{
			return g_mouseCollisionConfig;
		}

		// --- Rope Tension Scene: Physical tension with pulsing rhythms ---
		static const AudioModuleConfig g_ropeTensionConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "rope_tension";
			config.tempo = 70.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {57}; // A minor
			config.rhythmPattern = RhythmPatternId::BassKick;
			config.instrumentType = InstrumentType::Sawtooth;
			config.ambientVolume = 0.25f;
			config.melodyVolume = 0.25f;
			config.bassVolume = 0.40f;
			config.percussionVolume = 0.25f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.6f;
			config.frictionMinTempo = 60.0f;
			config.frictionMaxTempo = 100.0f;
			config.randomizeMelody = false;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getRopeTensionConfig()
		{
			return g_ropeTensionConfig;
		}

		// --- Collision Handling Scene: Technical precision with clean beats ---
		static const AudioModuleConfig g_collisionHandlingConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "collision_handling";
			config.tempo = 78.0f;
			config.scaleType = AudioScaleType::Pentatonic;
			config.scaleRoots = {67}; // G major pentatonic
			config.rhythmPattern = RhythmPatternId::Metronome;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.20f;
			config.melodyVolume = 0.4f;
			config.bassVolume = 0.3f;
			config.percussionVolume = 0.25f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.5f;
			config.frictionMinTempo = 65.0f;
			config.frictionMaxTempo = 110.0f;
			config.randomizeMelody = false;
			config.randomizeRhythm = false;
			config.activitySustainSec = 8.0f;
			config.activityFadeSpeed = 0.08f;
			return config;
		}();

		const AudioModuleConfig& getCollisionHandlingConfig()
		{
			return g_collisionHandlingConfig;
		}

		// --- Walk Exploration Scene: Discovery with evolving melodies ---
		static const AudioModuleConfig g_walkExplorationConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "walk_exploration";
			config.tempo = 62.0f;
			config.scaleType = AudioScaleType::Major;
			config.scaleRoots = {69}; // A major
			config.rhythmPattern = RhythmPatternId::OffBeat;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.30f;
			config.melodyVolume = 0.45f;
			config.bassVolume = 0.25f;
			config.percussionVolume = 0.12f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = true;
			config.frictionSensitivity = 0.4f;
			config.frictionMinTempo = 55.0f;
			config.frictionMaxTempo = 90.0f;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 9.0f;
			config.activityFadeSpeed = 0.07f;
			return config;
		}();

		const AudioModuleConfig& getWalkExplorationConfig()
		{
			return g_walkExplorationConfig;
		}

		// --- Text Narrative Scene: Storytelling with lyrical melodies ---
		static const AudioModuleConfig g_textNarrativeConfig = []() -> AudioModuleConfig
		{
			AudioModuleConfig config;
			config.sceneName = "text_narrative";
			config.tempo = 52.0f;
			config.scaleType = AudioScaleType::Minor;
			config.scaleRoots = {57}; // A minor
			config.rhythmPattern = RhythmPatternId::MelodicArpeggio;
			config.instrumentType = InstrumentType::Sine;
			config.ambientVolume = 0.40f;
			config.melodyVolume = 0.5f;
			config.bassVolume = 0.2f;
			config.percussionVolume = 0.06f;
			config.baseVolume = 0.6f;
			config.useFrictionModulation = false;
			config.randomizeMelody = true;
			config.randomizeRhythm = false;
			config.activitySustainSec = 10.0f;
			config.activityFadeSpeed = 0.06f;
			return config;
		}();

		const AudioModuleConfig& getTextNarrativeConfig()
		{
			return g_textNarrativeConfig;
		}

		// =====================================================================
		// Presets Registry & Lookup Functions
		// =====================================================================

		static std::unordered_map<std::string, const AudioModuleConfig*> s_presetRegistry;
		static bool s_presetsInitialized = false;

		void initAudioPresets()
		{
			if (s_presetsInitialized)
				return;

			s_presetRegistry["aquarium"] = &getAquariumConfig();
			s_presetRegistry["showcase"] = &getShowcaseConfig();
			s_presetRegistry["service-showcase"] = &getShowcaseConfig();
			s_presetRegistry["shapes"] = &getShapesConfig();
			s_presetRegistry["rope"] = &getRopeConfig();
			s_presetRegistry["life"] = &getLifeConfig();
			s_presetRegistry["walk"] = &getWalkConfig();
			s_presetRegistry["text"] = &getTextConfig();
			s_presetRegistry["image"] = &getImageConfig();
			s_presetRegistry["destroy"] = &getDestroyConfig();
			s_presetRegistry["destroy-test"] = &getDestroyConfig();
			s_presetRegistry["mouse_collision"] = &getMouseCollisionConfig();
			s_presetRegistry["cursor-collision"] = &getMouseCollisionConfig();
			s_presetRegistry["rope_tension"] = &getRopeTensionConfig();
			s_presetRegistry["collision_handling"] = &getCollisionHandlingConfig();
			s_presetRegistry["collision-handling"] = &getCollisionHandlingConfig();
			s_presetRegistry["walk_exploration"] = &getWalkExplorationConfig();
			s_presetRegistry["text_narrative"] = &getTextNarrativeConfig();

			s_presetsInitialized = true;
		}

		const AudioModuleConfig* getAudioPreset(const std::string& name)
		{
			initAudioPresets();
			auto it = s_presetRegistry.find(name);
			if (it != s_presetRegistry.end())
			{
				return it->second;
			}
			return nullptr;
		}

		std::vector<std::string> listAudioPresets()
		{
			initAudioPresets();
			std::vector<std::string> names;
			names.reserve(s_presetRegistry.size());
			for (const auto& [name, _] : s_presetRegistry)
			{
				names.push_back(name);
			}
			return names;
		}

		void setAudioModuleFromPreset(AudioModule* module, const std::string& presetName)
		{
			if (!module)
				return;

			const AudioModuleConfig* config = getAudioPreset(presetName);
			if (config)
			{
				module->initialize(*config);
			}
			else
			{
				AudioModuleConfig def;
				def.sceneName = presetName;
				module->initialize(def);
			}
		}

	} // namespace WeirdRenderer
} // namespace WeirdEngine
