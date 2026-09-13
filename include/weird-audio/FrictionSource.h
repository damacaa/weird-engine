#pragma once

#include "weird-engine/vec.h"
#include <cstdint>

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		// One continuous friction source submitted by a scene each frame (usually
		// one per contacted rigidbody). The id identifies the source entity/body
		// for debugging and inspection; voices are spatialized via cell binning.
		struct FrictionSource
		{
			uint32_t id = 0;
			vec3 position = vec3(0.0f);
			float level = 0.0f;
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
