#include "shape-editor/services/AudioPreviewManager.h"

#include <algorithm>

namespace WeirdEngine::Tools::ShapeEd
{
	const char* AudioPreviewManager::getScaleName(WeirdAudio::MusicalScale scale)
	{
		return WeirdAudio::SdfSong::getScaleName(scale);
	}

	std::string AudioPreviewManager::midiToNoteString(int midi)
	{
		return WeirdAudio::SdfSong::midiToNoteString(midi);
	}

	const char* AudioPreviewManager::getWaveTypeName(WeirdAudio::WaveType waveType)
	{
		return WeirdAudio::SdfMusicEngine::getWaveTypeName(waveType);
	}

	const char* AudioPreviewManager::getDrumKitName(int kit)
	{
		return WeirdAudio::SdfMusicEngine::getDrumKitName(kit);
	}

	const char* AudioPreviewManager::getLeadStyleName(WeirdAudio::LeadStyle style)
	{
		return WeirdAudio::SdfMusicEngine::getLeadStyleName(style);
	}

	Entity AudioPreviewManager::syncSong(ServiceProvider& services, const Expr& expr, glm::vec2 uiCenter,
										 const std::array<float, 8>& params)
	{
		const bool isNewSong = !m_song;
		bool topologyChanged = false;
		if (m_song)
		{
			const auto prevFp = m_song->getFingerprint();
			if (m_song->getCenter() != uiCenter)
			{
				m_song->setCenter(uiCenter);
			}
			m_song->setShapeExpression(expr.node);
			for (size_t i = 0; i < 8; ++i)
			{
				if (m_song->getParameter(i) != params[i])
				{
					m_song->setParameter(i, params[i]);
				}
			}
			m_song->calculateMusicalPropertiesFromShape();
			const auto& nextFp = m_song->getFingerprint();
			topologyChanged = (prevFp.structuralHash != nextFp.structuralHash || prevFp.nodeCount != nextFp.nodeCount ||
							   prevFp.maxDepth != nextFp.maxDepth || prevFp.branchCount != nextFp.branchCount);
		}
		else
		{
			m_song = WeirdAudio::SdfSong::create("node_editor_song", expr, uiCenter);
			for (size_t i = 0; i < 8; ++i)
			{
				if (m_song->getParameter(i) != params[i])
				{
					m_song->setParameter(i, params[i]);
				}
			}
		}

		SongVisualizationOptions visualOptions;
		visualOptions.mode = SongVisualizationMode::UI;
		visualOptions.material = 0;
		visualOptions.combination = CombinationType::Addition;
		visualOptions.group = 0;

		Entity previewEntity = services.audio().setSong(m_song, visualOptions, false);
		if (previewEntity != INVALID_ENTITY && services.registry().hasComponent<UIShape>(previewEntity))
		{
			auto& uiShape = services.registry().getComponent<UIShape>(previewEntity);
			uiShape.smoothFactor = 0.0f;
			uiShape.material = 0;
			std::copy_n(params.data(), 8, uiShape.parameters);
			services.registry().setComponentDirty(uiShape);
		}

		if (!isNewSong)
		{
			services.audio().resampleShape();
			if (topologyChanged)
			{
				services.audio().music().resetInstrumentRackToShape();
			}
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

	std::shared_ptr<WeirdAudio::SdfSong> AudioPreviewManager::getSong() const
	{
		return m_song;
	}
} // namespace WeirdEngine::Tools::ShapeEd
