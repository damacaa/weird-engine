#pragma once

#include <array>
#include <memory>
#include <string>

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

namespace WeirdEngine::Editor
{
	class AudioPreviewManager
	{
	public:
		static const char* getScaleName(WeirdAudio::MusicalScale scale);
		static std::string midiToNoteString(int midi);
		static const char* getWaveTypeName(WeirdAudio::WaveType waveType);
		static const char* getDrumKitName(int kit);

		AudioPreviewManager() = default;

		Entity syncSong(ServiceProvider& services, const Expr& expr, glm::vec2 uiCenter,
						const std::array<float, 8>& params);
		void updateAudioState(ServiceProvider& services);

		bool isPlaying() const;
		void setPlaying(ServiceProvider& services, bool play);

		float getVolume() const;
		void setVolume(ServiceProvider& services, float volume);

		std::shared_ptr<WeirdAudio::SdfSong> getSong() const;

	private:
		std::shared_ptr<WeirdAudio::SdfSong> m_song;
		bool m_playSong = true;
		float m_musicVolume = 0.75f;
	};
} // namespace WeirdEngine::Editor
