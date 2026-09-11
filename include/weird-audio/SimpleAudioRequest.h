#pragma once

#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		struct SimpleAudioRequest
		{
			float volume = 0.5f;
			float frequency = 0.0f; // 0.0f indicates auto-quantize to current active chord
			bool spatial = false;
			vec3 position = vec3(0.0f);
			int beats = 1;
			float intensity = 0.5f; // Impact energy 0.0 (light) to 1.0 (heavy)
			int instrument = 0;		// 0 = Sine, 1 = Sawtooth, 2 = Square, 3 = Noise, 4 = Polyphonic
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine