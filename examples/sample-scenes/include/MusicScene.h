#pragma once

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>

namespace MusicSceneNamespace
{
	struct State
	{
		static constexpr size_t NUM_SONGS = 4;
		static constexpr size_t NUM_DYNAMICS = 2;
		static constexpr size_t NUM_GAUGE_BARS = 5;
		static constexpr size_t NUM_TRACKS = 4;

		enum DynamicIndex : size_t
		{
			DYN_SURGE = 0,
			DYN_DUCK = 1
		};

		std::array<WeirdEngine::Entity, NUM_SONGS> songToggles{WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
															   WeirdEngine::INVALID_ENTITY,
															   WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Material2DHandle, NUM_SONGS> songMats{};

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
		std::array<WeirdEngine::Entity, 10> uiPoints{};

		bool selectionMode = false;
		int cursorSongIndex = 0;
		int confirmedSongIndex = -1;
		WeirdEngine::Entity cursorDot = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Material2DHandle cursorMat{};

		// In-scene Surge & Duck dynamic labels, animated 5-bar equalizers, and track mixer
		std::array<WeirdEngine::Entity, NUM_DYNAMICS> dynamicLabels{WeirdEngine::INVALID_ENTITY,
																	WeirdEngine::INVALID_ENTITY};
		std::array<float, NUM_DYNAMICS> dynamicFlashTimers{};

		std::array<WeirdEngine::Entity, NUM_TRACKS> trackToggles{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Entity, NUM_TRACKS> trackLabels{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY};
		std::array<uint8_t, NUM_TRACKS> trackVisualActive{1, 1, 1, 1};

		std::array<WeirdEngine::Entity, NUM_GAUGE_BARS> surgeTrackBars{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Entity, NUM_GAUGE_BARS> surgeFillBars{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Entity, NUM_GAUGE_BARS> duckTrackBars{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};
		std::array<WeirdEngine::Entity, NUM_GAUGE_BARS> duckFillBars{
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY,
			WeirdEngine::INVALID_ENTITY, WeirdEngine::INVALID_ENTITY};

		WeirdEngine::Material2DHandle playMat{};
		WeirdEngine::Material2DHandle pauseMat{};
		WeirdEngine::Material2DHandle textMat{};
		WeirdEngine::Material2DHandle surgeMat{};
		WeirdEngine::Material2DHandle duckMat{};
		WeirdEngine::Material2DHandle trackOnMat{};
		WeirdEngine::Material2DHandle trackOffMat{};
		WeirdEngine::Material2DHandle dimTextMat{};

		int lastStatusCode = -1;
	};

	static_assert(WeirdEngine::is_snapshot_serializable_v<State>);

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupSongsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupTogglesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupControlsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupVisualizerDotsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupWorldSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);

	void toggleSelectionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void playbackControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void dynamicControlsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void parameterControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void statusTextSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void songLayoutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void uiDotsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void updateWorldVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
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
		addStartSystem(MusicSceneNamespace::setupWorldSystem);

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(MusicSceneNamespace::toggleSelectionSystem);
		addUpdateSystem(MusicSceneNamespace::playbackControlSystem);
		addUpdateSystem(MusicSceneNamespace::dynamicControlsSystem);
		addUpdateSystem(MusicSceneNamespace::parameterControlSystem);
		addUpdateSystem(MusicSceneNamespace::statusTextSystem);
		addUpdateSystem(MusicSceneNamespace::songLayoutSystem);
		addUpdateSystem(MusicSceneNamespace::uiDotsSystem);
		addUpdateSystem(MusicSceneNamespace::updateWorldVisualsSystem);
	}

	void onImGuiRender(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services) override;
};
