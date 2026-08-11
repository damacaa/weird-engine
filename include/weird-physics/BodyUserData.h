#pragma once

namespace WeirdEngine
{
	// Base class for per-body user data attached to rigid bodies via
	// Simulation2D::setUserData(). The simulation only stores the pointer and
	// never interprets it: derive from this, set `type` to a game-specific
	// discriminator, and use Simulation2D::getUserDataAs<T>() (which checks
	// `T::TYPE` against `type` before casting) or check `type` manually.
	//
	// Set `type` in the derived class's constructor (e.g.
	// `MyData() { type = TYPE; }`) so the discriminator can never drift out
	// of sync with T::TYPE.
	//
	// Ownership is transferred to the simulation with std::unique_ptr (e.g.
	// std::make_unique<MyData>()): it deletes the data when the body is
	// removed and deletes all remaining data when the simulation is destroyed
	// (scene teardown). Do not retain the pointer after setUserData(); query
	// it back via getUserData()/getUserDataAs<T>() instead.
	struct BodyUserData
	{
		int type = 0;
	};
} // namespace WeirdEngine
