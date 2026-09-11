#pragma once

namespace WeirdEngine
{
	namespace WeirdAudio
	{
		struct AudioSettings
		{
			bool mute = false;
			bool enableAmbient = true;
			bool enableMusic = true;
			bool enablePhysicsAudio = true;
			bool enableSpatialAudio = true; // Setting to toggle spatial sound on/off
			float masterVolume = 1.0f;
			float physicsVolume = 0.8f;
			float musicVolume = 0.65f;
		};
	} // namespace WeirdAudio
} // namespace WeirdEngine
