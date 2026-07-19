#pragma once

#include <limits>

namespace WeirdEngine
{
	struct GlobalPhysicsSettings
	{
		GlobalPhysicsSettings()
			: gravity(0.0f)
			, damping(0.05f) {};

		float gravity;
		float damping;
	};
} // namespace WeirdEngine
