#pragma once

#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace WeirdRenderer
	{
		struct SimpleAudioRequest
		{
			float volume;
			float frequency;
			bool spatial;
			vec3 position;
			int beats = 1;
		};
	} // namespace WeirdRenderer
} // namespace WeirdEngine