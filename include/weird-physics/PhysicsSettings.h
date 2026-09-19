
#pragma once
namespace WeirdEngine
{
	struct PhysicsSettings
	{
		float gravity = -9.8f;
		float damping = 0.001f;
		// Tangential friction coefficient for sphere-sphere contacts.
		// 0.05 slides, 0.2 is golf-turf-like, 0.5 is grippy.
		float friction = 0.2f;
		float simulationFrequency = 100.0f;
		int relaxationSteps = 10;
		bool runSimulationInThread = true;
	};
} // namespace WeirdEngine
