#include "services/AudioPreviewManager.h"

#include <algorithm>

namespace WeirdEngine::Editor
{
	const char* AudioPreviewManager::getScaleName(WeirdRenderer::MusicalScale scale)
	{
		switch (scale)
		{
			case WeirdRenderer::MusicalScale::PentatonicMajor:
				return "Pentatonic Major";
			case WeirdRenderer::MusicalScale::PentatonicMinor:
				return "Pentatonic Minor";
			case WeirdRenderer::MusicalScale::Major:
				return "Major";
			case WeirdRenderer::MusicalScale::NaturalMinor:
				return "Natural Minor";
			case WeirdRenderer::MusicalScale::Dorian:
				return "Dorian";
			case WeirdRenderer::MusicalScale::Lydian:
				return "Lydian";
			case WeirdRenderer::MusicalScale::Chromatic:
				return "Chromatic";
			default:
				return "Unknown";
		}
	}

	std::string AudioPreviewManager::midiToNoteString(int midi)
	{
		if (midi < 0 || midi > 127)
			return "N/A";
		const char* noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
		int note = midi % 12;
		int octave = (midi / 12) - 1;
		return std::string(noteNames[note]) + std::to_string(octave);
	}

	const char* AudioPreviewManager::getWaveTypeName(WeirdRenderer::WaveType waveType)
	{
		switch (waveType)
		{
			case WeirdRenderer::WaveType::SoftSine:
				return "Soft Sine (Warm)";
			case WeirdRenderer::WaveType::BandlimitedSaw:
				return "Bandlimited Saw (Bright)";
			case WeirdRenderer::WaveType::PulseSquare:
				return "Pulse Square (Reedy)";
			case WeirdRenderer::WaveType::FMPluck:
				return "FM Pluck (Bell/Metallic)";
			case WeirdRenderer::WaveType::Wavefolder:
				return "Wavefolder (Buchla/Evolving)";
			default:
				return "Unknown";
		}
	}

	const char* AudioPreviewManager::getDrumKitName(int kit)
	{
		switch (kit)
		{
			case 1:
				return "Acoustic Punch";
			case 2:
				return "Industrial / 909";
			case 0:
			default:
				return "Deep 808 Electronic";
		}
	}

	Entity AudioPreviewManager::syncSong(ServiceProvider& services, const Expr& expr, glm::vec2 uiCenter,
										 const std::array<float, 8>& params)
	{
		if (m_song)
		{
			m_song->setCenter(uiCenter);
			m_song->setShapeExpression(expr.node);
			for (size_t i = 0; i < 8; ++i)
			{
				m_song->setParameter(i, params[i]);
			}
			m_song->calculateMusicalPropertiesFromShape();
		}
		else
		{
			m_song = WeirdRenderer::SdfSong::create("node_editor_song", expr, uiCenter);
			for (size_t i = 0; i < 8; ++i)
			{
				m_song->setParameter(i, params[i]);
			}
		}

		SongVisualizationOptions visualOptions;
		visualOptions.material = 0;
		visualOptions.combination = CombinationType::Addition;
		visualOptions.group = 0;

		Entity previewEntity = services.audio().setSong(m_song, visualOptions);
		if (previewEntity != INVALID_ENTITY && services.registry().hasComponent<UIShape>(previewEntity))
		{
			auto& uiShape = services.registry().getComponent<UIShape>(previewEntity);
			uiShape.smoothFactor = 0.0f;
			uiShape.material = 0;
			std::copy_n(params.data(), 8, uiShape.parameters);
			services.registry().setComponentDirty(uiShape);
		}

		return previewEntity;
	}

	void AudioPreviewManager::updateAudioState(ServiceProvider& services)
	{
		services.audio().music().setPlaying(m_playSong);
		services.audio().music().setVolume(m_musicVolume);
	}

	bool AudioPreviewManager::isPlaying() const
	{
		return m_playSong;
	}

	void AudioPreviewManager::setPlaying(ServiceProvider& services, bool play)
	{
		m_playSong = play;
		services.audio().music().setPlaying(m_playSong);
	}

	float AudioPreviewManager::getVolume() const
	{
		return m_musicVolume;
	}

	void AudioPreviewManager::setVolume(ServiceProvider& services, float volume)
	{
		m_musicVolume = volume;
		services.audio().music().setVolume(m_musicVolume);
	}

	std::shared_ptr<WeirdRenderer::SdfSong> AudioPreviewManager::getSong() const
	{
		return m_song;
	}
} // namespace WeirdEngine::Editor
