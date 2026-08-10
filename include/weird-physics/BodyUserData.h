#pragma once

namespace WeirdEngine
{
	// Base class for per-body user data attached to rigid bodies via
	// Simulation2D::setUserData(). The simulation only stores the pointer and
	// never interprets it: derive from this, set `type` to a game-specific
	// discriminator, and use Simulation2D::getUserDataAs<T>() (which checks
	// `T::TYPE` against `type` before casting) or check `type` manually.
	//
	// The pointer must be heap-allocated with `new`. The simulation takes
	// ownership: it deletes the data when the body is removed and deletes all
	// remaining data when the simulation is destroyed (scene teardown).
	struct BodyUserData
	{
		int type = 0;
	};
} // namespace WeirdEngine
