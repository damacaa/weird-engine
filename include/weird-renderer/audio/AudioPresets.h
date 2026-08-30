#pragma once

#include "weird-renderer/audio/AudioModule.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{

		// =====================================================================
		// Scene-Specific Audio Presets - Defined Through Code (No Files)
		// =====================================================================

		// --- Aquarium Scene: Calming underwater ambience with gentle bubbles ---
		const AudioModuleConfig& getAquariumConfig();

		// --- Service Showcase: Dynamic physics demo with energetic beats ---
		const AudioModuleConfig& getShowcaseConfig();

		// --- Shapes Combinations: Creative exploration with playful melodies ---
		const AudioModuleConfig& getShapesConfig();

		// --- Rope Scene: Tension and release with rhythmic patterns ---
		const AudioModuleConfig& getRopeConfig();

		// --- Life Scene: Organic growth with natural rhythms ---
		const AudioModuleConfig& getLifeConfig();

		// --- Walk Scene: Journey and movement with steady rhythm ---
		const AudioModuleConfig& getWalkConfig();

		// --- Text Scene: Narrative flow with melodic storytelling ---
		const AudioModuleConfig& getTextConfig();

		// --- Image Scene: Visual exploration with atmospheric pads ---
		const AudioModuleConfig& getImageConfig();

		// --- Destroy Scene: Intense action with driving rhythm ---
		const AudioModuleConfig& getDestroyConfig();

		// --- Mouse Collision Scene: Interactive feedback with responsive beats ---
		const AudioModuleConfig& getMouseCollisionConfig();

		// --- Rope Tension Scene: Physical tension with pulsing rhythms ---
		const AudioModuleConfig& getRopeTensionConfig();

		// --- Collision Handling Scene: Technical precision with clean beats ---
		const AudioModuleConfig& getCollisionHandlingConfig();

		// --- Walk Exploration Scene: Discovery with evolving melodies ---
		const AudioModuleConfig& getWalkExplorationConfig();

		// --- Text Narrative Scene: Storytelling with lyrical melodies ---
		const AudioModuleConfig& getTextNarrativeConfig();

	} // namespace WeirdRenderer
} // namespace WeirdEngine
