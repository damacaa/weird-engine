#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <weird-engine.h>

namespace MusicSceneNamespace
{
	struct State
	{
		static constexpr size_t NUM_SONGS = 4;

		std::array<std::shared_ptr<WeirdEngine::WeirdAudio::SdfSong>, NUM_SONGS> songs;
		std::array<WeirdEngine::Entity, NUM_SONGS> songToggles;
		std::array<WeirdEngine::Material2DHandle, NUM_SONGS> songMats;

		int selectedSongIndex = -1;
		int currentLoadedSongIndex = -1;
		bool isPlaying = false;

		float paramSize = 26.0f; // Parameter 0: Overall size / volume
		float paramSpeed = 1.0f; // Parameter 1: Speed / tempo

		WeirdEngine::Entity playButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity pauseButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity statusText = WeirdEngine::INVALID_ENTITY;

		WeirdEngine::Entity backgroundShape = WeirdEngine::INVALID_ENTITY;

		int lastResolutionHash = 0;
		std::vector<WeirdEngine::Entity> uiPoints;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupSongsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupTogglesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupControlsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupVisualizerDotsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupWorldSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);

	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void toggleSelectionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void playbackControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void parameterControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void statusTextSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void songLayoutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void uiDotsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void updateWorldVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);

	void onDestroySystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace MusicSceneNamespace

class MusicScene : public WeirdEngine::Scene2D
{
public:
	MusicScene()
	{
		addStartSystem(MusicSceneNamespace::stateInitSystem);
		addStartSystem(MusicSceneNamespace::setupSongsSystem);
		addStartSystem(MusicSceneNamespace::setupTogglesSystem);
		addStartSystem(MusicSceneNamespace::setupControlsSystem);
		addStartSystem(MusicSceneNamespace::setupVisualizerDotsSystem);
		addStartSystem(MusicSceneNamespace::cameraInitSystem);
		addStartSystem(MusicSceneNamespace::setupWorldSystem);

		addUpdateSystem(MusicSceneNamespace::sceneControlSystem);
		addUpdateSystem(MusicSceneNamespace::toggleSelectionSystem);
		addUpdateSystem(MusicSceneNamespace::playbackControlSystem);
		addUpdateSystem(MusicSceneNamespace::parameterControlSystem);
		addUpdateSystem(MusicSceneNamespace::statusTextSystem);
		addUpdateSystem(MusicSceneNamespace::songLayoutSystem);
		addUpdateSystem(MusicSceneNamespace::uiDotsSystem);
		addUpdateSystem(MusicSceneNamespace::cameraTrackingSystem);
		addUpdateSystem(MusicSceneNamespace::updateWorldVisualsSystem);

		addDestroySystem(MusicSceneNamespace::onDestroySystem);
	}
};
