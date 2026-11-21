#include "MusicScene.h"
#include "MusicShapes.h"

#include <cmath>
#include <cstdio>
#ifndef WEIRD_DISABLE_IMGUI
#include <imgui.h>
#endif

using namespace WeirdEngine;

namespace MusicSceneNamespace
{
	static std::array<std::shared_ptr<WeirdEngine::WeirdAudio::SdfSong>, State::NUM_SONGS> s_songs;
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "MusicScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	using namespace SDF;

	void setupSongsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(false);
		services.debug().setDebugFly(false);

		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		glm::vec2 previewCenter(winW * 0.5f, winH * 0.5f);
		state.lastResolutionHash = (Display::width * 73856093) ^ Display::height;

		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			Material2D& mat = services.materials2D().createMaterial("music_mat_" + std::to_string(i));
			mat.color = ColorPalette::Default[8 + i % ColorPalette::Default.size()];
			state.songMats[i] = mat.id;
		}

		// 3. Instantiate the 4 songs using shared MusicShapes definitions
		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			s_songs[i] = MusicShapes::createSong(i, previewCenter);
		}

		auto activeSong = services.audio().music().getCurrentSong();
		int activeIdx = -1;
		if (activeSong && services.audio().music().getVolume() > 0.0f)
		{
			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (s_songs[i]->getName() == activeSong->getName())
				{
					s_songs[i] = activeSong;
					activeIdx = static_cast<int>(i);
					break;
				}
			}
		}

		if (activeIdx >= 0)
		{
			state.selectedSongIndex = activeIdx;
			state.confirmedSongIndex = activeIdx;
			state.cursorSongIndex = activeIdx;
			state.currentLoadedSongIndex = activeIdx;
			state.isPlaying = true;
			state.paramSize = activeSong->getParameter(0);
			state.paramSpeed = activeSong->getParameter(1);
			s_songs[activeIdx]->setCenter(previewCenter);
			services.audio().setSong(
				s_songs[activeIdx],
				SongVisualizationOptions{.mode = SongVisualizationMode::UI, .material = state.songMats[activeIdx]},
				false);
		}
		else
		{
			state.selectedSongIndex = -1;
			state.confirmedSongIndex = -1;
			state.cursorSongIndex = 0;
			services.audio().setSong(nullptr, false);
			services.audio().music().setTrackToggles({false, false, false, false});
			services.audio().music().setVolume(0.0f);
		}
	}

	struct MusicUiLayout
	{
		float winW = 800.0f;
		float winH = 800.0f;
		float edgePad = 32.0f;
		float rightEdgePad = 24.0f;
		float leftPillX = 32.0f;
		float leftLabelX = 55.0f;
		float rightLabelX = 776.0f;
		float gaugeLeftX = 595.5f;
		float gaugePitchX = 42.0f;
		float gaugeHalfW = 12.5f;
		float gaugeMaxH = 34.0f;
		float toggleY = 745.0f;
		float botY = 48.0f;
		float pillHalfSize = 13.0f;
		std::array<float, State::NUM_DYNAMICS> dynamicY{};
		std::array<float, State::NUM_DYNAMICS> gaugeBaseY{};
		std::array<float, State::NUM_TRACKS> trackY{};
	};

	static MusicUiLayout computeMusicUiLayout()
	{
		MusicUiLayout l;
		l.winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		l.winH = static_cast<float>(Display::height > 0 ? Display::height : 800);

		const float t = std::clamp((l.winH - 480.0f) / 320.0f, 0.0f, 1.0f);
		l.edgePad = (l.winW >= 760.0f) ? 32.0f : 22.0f;
		l.rightEdgePad = (l.winW >= 760.0f) ? 24.0f : 16.0f;
		l.pillHalfSize = (l.winW >= 760.0f) ? 13.0f : 11.0f;

		l.leftPillX = l.edgePad;
		l.leftLabelX = l.leftPillX + l.pillHalfSize + 10.0f;

		l.rightLabelX = l.winW - l.rightEdgePad;
		l.gaugePitchX = 36.0f + 6.0f * t;
		l.gaugeHalfW = 10.0f + 2.5f * t;
		const float gaugeRightX = l.rightLabelX - l.gaugeHalfW;
		l.gaugeLeftX = gaugeRightX - static_cast<float>(State::NUM_GAUGE_BARS - 1) * l.gaugePitchX;
		l.gaugeMaxH = 24.0f + 10.0f * t;

		l.toggleY = l.winH - ((l.winH < 540.0f) ? 44.0f : 55.0f);
		l.botY = (l.winH < 540.0f) ? 38.0f : 48.0f;

		const float leftPitch = 53.0f + 15.0f * t;
		const float leftTopY = l.winH * 0.5f + 1.5f * leftPitch;
		for (size_t i = 0; i < State::NUM_TRACKS; ++i)
		{
			l.trackY[i] = leftTopY - static_cast<float>(i) * leftPitch;
		}

		l.dynamicY[State::DYN_SURGE] = l.winH * 0.5f + 75.0f + 40.0f * t;
		l.dynamicY[State::DYN_DUCK] = l.dynamicY[State::DYN_SURGE] - (140.0f + 30.0f * t);
		const float gaugeOffset = 68.0f + 14.0f * t;
		l.gaugeBaseY[State::DYN_SURGE] = l.dynamicY[State::DYN_SURGE] - gaugeOffset;
		l.gaugeBaseY[State::DYN_DUCK] = l.dynamicY[State::DYN_DUCK] - gaugeOffset;

		return l;
	}

	static void updateEqualizerGauge(Registry& registry, const std::array<Entity, State::NUM_GAUGE_BARS>& trackBars,
									 const std::array<Entity, State::NUM_GAUGE_BARS>& fillBars, const MusicUiLayout& l,
									 float baseY, float level, float audioVol, float timeSec, bool ascending)
	{
		const float clamped = std::clamp(level, 0.0f, 1.0f);
		for (size_t i = 0; i < State::NUM_GAUGE_BARS; ++i)
		{
			const float stepNorm = static_cast<float>(i) / static_cast<float>(State::NUM_GAUGE_BARS - 1);
			const float profile = ascending ? (0.42f + 0.58f * stepNorm) : (1.0f - 0.58f * stepNorm);
			const float bx = l.gaugeLeftX + static_cast<float>(i) * l.gaugePitchX;

			if (registry.isEntityValid(trackBars[i]) && registry.hasComponent<UIShape>(trackBars[i]))
			{
				const float trackHalfH = 3.5f + profile * 4.5f;
				auto& tr = registry.getComponent<UIShape>(trackBars[i]);
				tr.parameters[DefaultShapes::RoundBox::PosX] = bx;
				tr.parameters[DefaultShapes::RoundBox::PosY] = baseY + trackHalfH;
				tr.parameters[DefaultShapes::RoundBox::SizeX] = l.gaugeHalfW;
				tr.parameters[DefaultShapes::RoundBox::SizeY] = trackHalfH;
				tr.parameters[DefaultShapes::RoundBox::Radius] = 3.0f;
			}

			if (registry.isEntityValid(fillBars[i]) && registry.hasComponent<UIShape>(fillBars[i]))
			{
				auto& fill = registry.getComponent<UIShape>(fillBars[i]);
				const float threshold = static_cast<float>(i) * 0.17f;
				const float activeFrac = std::clamp((clamped - threshold) / 0.28f, 0.0f, 1.0f);
				if (activeFrac <= 0.01f)
				{
					fill.parameters[DefaultShapes::RoundBox::PosX] = -1000.0f;
					fill.parameters[DefaultShapes::RoundBox::PosY] = -1000.0f;
					fill.parameters[DefaultShapes::RoundBox::SizeX] = 4.0f;
					fill.parameters[DefaultShapes::RoundBox::SizeY] = 4.0f;
					fill.parameters[DefaultShapes::RoundBox::Radius] = 2.0f;
				}
				else
				{
					const float wave = 0.88f + 0.18f * (0.4f + audioVol) *
												   std::sin(timeSec * (7.5f + static_cast<float>(i) * 1.7f) +
															static_cast<float>(i) * 1.15f);
					const float fullH = (l.gaugeMaxH * profile) * (0.28f + 0.72f * activeFrac) * wave;
					const float halfH = (std::max)(4.0f, 0.5f * fullH);
					fill.parameters[DefaultShapes::RoundBox::PosX] = bx;
					fill.parameters[DefaultShapes::RoundBox::PosY] = baseY + halfH;
					fill.parameters[DefaultShapes::RoundBox::SizeX] = l.gaugeHalfW;
					fill.parameters[DefaultShapes::RoundBox::SizeY] = halfH;
					fill.parameters[DefaultShapes::RoundBox::Radius] = 3.5f;
				}
			}
		}
	}

	static void setPillLayout(Registry& registry, Entity pillEntity, Entity labelEntity, float px, float lx, float y,
							  float halfSize)
	{
		if (registry.isEntityValid(pillEntity) && registry.hasComponent<UIShape>(pillEntity))
		{
			auto& shape = registry.getComponent<UIShape>(pillEntity);
			shape.parameters[DefaultShapes::RoundBox::PosX] = px;
			shape.parameters[DefaultShapes::RoundBox::PosY] = y;
			shape.parameters[DefaultShapes::RoundBox::SizeX] = halfSize;
			shape.parameters[DefaultShapes::RoundBox::SizeY] = halfSize;
		}

		if (registry.isEntityValid(labelEntity) && registry.hasComponent<Transform>(labelEntity))
		{
			auto& t = registry.getComponent<Transform>(labelEntity);
			t.position = vec3(lx, y, 0.0f);
		}
	}

	static void updateCursorDot(Registry& registry, State& state)
	{
		if (!registry.isEntityValid(state.cursorDot) || !registry.hasComponent<Transform>(state.cursorDot))
		{
			return;
		}

		auto& t = registry.getComponent<Transform>(state.cursorDot);
		if (state.selectionMode && state.cursorSongIndex >= 0 &&
			state.cursorSongIndex < static_cast<int>(State::NUM_SONGS))
		{
			const MusicUiLayout l = computeMusicUiLayout();
			const float spacingX = 60.0f;
			const float totalWidth = spacingX * 3;
			const float startX = (l.winW * 0.5f) - (totalWidth * 0.5f);

			t.position = vec3(startX + static_cast<float>(state.cursorSongIndex) * spacingX, l.toggleY - 36.0f, 0.0f);
		}
		else
		{
			t.position = vec3(-1000.0f, -1000.0f, 0.0f);
		}
	}

	static void applyMusicUiLayout(Registry& registry, State& state, ServiceProvider& services)
	{
		const MusicUiLayout l = computeMusicUiLayout();

		if (registry.isEntityValid(state.playButton) && registry.hasComponent<UIShape>(state.playButton))
		{
			auto& s = registry.getComponent<UIShape>(state.playButton);
			s.parameters[DefaultShapes::TriangleRotated::PosX] = 44.0f;
			s.parameters[DefaultShapes::TriangleRotated::PosY] = l.botY;
		}
		if (registry.isEntityValid(state.pauseButton) && registry.hasComponent<UIShape>(state.pauseButton))
		{
			auto& s = registry.getComponent<UIShape>(state.pauseButton);
			s.parameters[DefaultShapes::Box::PosX] = 92.0f;
			s.parameters[DefaultShapes::Box::PosY] = l.botY;
		}
		if (registry.isEntityValid(state.statusText) && registry.hasComponent<Transform>(state.statusText))
		{
			registry.getComponent<Transform>(state.statusText).position = vec3(136.0f, l.botY, 0.0f);
		}

		for (size_t i = 0; i < State::NUM_DYNAMICS; ++i)
		{
			if (registry.isEntityValid(state.dynamicLabels[i]) &&
				registry.hasComponent<Transform>(state.dynamicLabels[i]))
			{
				registry.getComponent<Transform>(state.dynamicLabels[i]).position =
					vec3(l.rightLabelX, l.dynamicY[i], 0.0f);
			}
		}
		for (size_t i = 0; i < State::NUM_TRACKS; ++i)
		{
			setPillLayout(registry, state.trackToggles[i], state.trackLabels[i], l.leftPillX, l.leftLabelX, l.trackY[i],
						  l.pillHalfSize);
		}

		const auto& musicEngine = services.audio().music();
		const float audioVol = state.isPlaying ? services.audio().getAudioVolume() : 0.0f;
		const float timeSec = services.time().time();
		updateEqualizerGauge(registry, state.surgeTrackBars, state.surgeFillBars, l, l.gaugeBaseY[State::DYN_SURGE],
							 musicEngine.getSurgeLevel(), audioVol, timeSec, true);
		updateEqualizerGauge(registry, state.duckTrackBars, state.duckFillBars, l, l.gaugeBaseY[State::DYN_DUCK],
							 musicEngine.getDuckingLevel(), audioVol, timeSec, false);
	}

	static void loadAndPlaySong(Registry& registry, ServiceProvider& services, State& state, int idx)
	{
		if (idx < 0 || idx >= static_cast<int>(State::NUM_SONGS) || !s_songs[idx])
			return;

		state.selectedSongIndex = idx;
		state.confirmedSongIndex = idx;
		state.cursorSongIndex = idx;
		state.currentLoadedSongIndex = idx;
		state.isPlaying = true;

		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			if (registry.isEntityValid(state.songToggles[i]) &&
				registry.hasComponent<ShapeToggle>(state.songToggles[i]))
			{
				registry.getComponent<ShapeToggle>(state.songToggles[i]).active = (static_cast<int>(i) == idx);
			}
		}

		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		glm::vec2 previewCenter(winW * 0.5f, winH * 0.5f);

		s_songs[idx]->setCenter(previewCenter);
		s_songs[idx]->setParameter(0, state.paramSize);
		s_songs[idx]->setParameter(1, state.paramSpeed);

		services.audio().setSong(s_songs[idx], {.mode = SongVisualizationMode::UI, .material = state.songMats[idx]},
								 false);
		services.audio().setSongParameter(0, state.paramSize);
		services.audio().setSongParameter(1, state.paramSpeed);
		services.audio().resampleShape();

		services.audio().music().setTrackToggles({true, true, true, true});
		services.audio().music().setVolume(0.65f);
		updateCursorDot(registry, state);
	}

	static bool isButtonEntityDown(Registry& registry, Entity entity)
	{
		if (!registry.isEntityValid(entity))
			return false;
		if (registry.hasComponent<ShapeButton>(entity) &&
			registry.getComponent<ShapeButton>(entity).state == ButtonState::Down)
			return true;
		if (registry.hasComponent<ShapeToggle>(entity) &&
			registry.getComponent<ShapeToggle>(entity).state == ButtonState::Down)
			return true;
		if (registry.hasComponent<TextButton>(entity) &&
			registry.getComponent<TextButton>(entity).state == ButtonState::Down)
			return true;
		if (registry.hasComponent<TextToggle>(entity) &&
			registry.getComponent<TextToggle>(entity).state == ButtonState::Down)
			return true;
		return false;
	}

	void setupTogglesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		const MusicUiLayout l = computeMusicUiLayout();
		const float spacingX = 60.0f;
		const float totalWidth = spacingX * 3;
		const float startX = (l.winW * 0.5f) - (totalWidth * 0.5f);

		auto boundToggle = [](Expr shape, Vec2Expr p)
		{
			// Intersect with a circle to prevent shapes (like unbounded sine waves) from bleeding across the UI
			return sdfIntersect(shape, sdCircle(p, Expr(var(0)) * 1.2f));
		};

		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			Vec2Expr p = translate(point(), {Expr(var(2)), Expr(var(3))});
			ShapeId id = services.shapes().registerSDF(boundToggle(MusicShapes::getSongShape(i, p), p));
			state.songToggles[i] = services.shapes().addUIShape(
				{.shapeId = id,
				 .variables = {{0, 16.0f}, {1, 1.0f}, {2, startX + static_cast<float>(i) * spacingX}, {3, l.toggleY}},
				 .material = state.songMats[i]});
			auto& toggle = registry.addComponent<ShapeToggle>(state.songToggles[i]);
			toggle.active = (state.selectedSongIndex == static_cast<int>(i));
			toggle.clickPadding = 6.0f;
			toggle.modifierAmount = 4.0f;
			toggle.parameterModifierMask.set(0);
		}

		Material2D& cursorMat = services.materials2D().createMaterial("music_cursor_mat");
		cursorMat.color = vec4(1.0f, 0.84f, 0.26f, 0.95f);
		state.cursorMat = cursorMat.id;

		state.cursorDot = registry.createEntity();
		auto& t = registry.addComponent<Transform>(state.cursorDot);
		t.position = vec3(-1000.0f, -1000.0f, 0.0f);
		auto& ui = registry.addComponent<UIDot>(state.cursorDot);
		ui.materialId = state.cursorMat;
	}

	void setupControlsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		services.audio().resetDynamicEffects();

		Material2D& playMat = services.materials2D().createMaterial("music_play_mat");
		playMat.color = vec4(0.2f, 0.95f, 0.3f, 0.95f);
		state.playMat = playMat.id;

		Material2D& pauseMat = services.materials2D().createMaterial("music_pause_mat");
		pauseMat.color = vec4(0.95f, 0.2f, 0.25f, 0.95f);
		state.pauseMat = pauseMat.id;

		Material2D& textMat = services.materials2D().createMaterial("music_text_mat");
		textMat.color = ColorPalette::White;
		state.textMat = textMat.id;

		Material2D& surgeMat = services.materials2D().createMaterial("music_surge_mat");
		surgeMat.color = vec4(0.15f, 0.92f, 1.0f, 0.98f);
		state.surgeMat = surgeMat.id;

		Material2D& duckMat = services.materials2D().createMaterial("music_duck_mat");
		duckMat.color = vec4(1.0f, 0.56f, 0.14f, 0.98f);
		state.duckMat = duckMat.id;

		Material2D& trackOnMat = services.materials2D().createMaterial("music_track_on_mat");
		trackOnMat.color = vec4(0.24f, 0.94f, 0.68f, 0.96f);
		state.trackOnMat = trackOnMat.id;

		Material2D& trackOffMat = services.materials2D().createMaterial("music_track_off_mat");
		trackOffMat.color = vec4(0.14f, 0.17f, 0.22f, 0.96f);
		state.trackOffMat = trackOffMat.id;

		Material2D& dimTextMat = services.materials2D().createMaterial("music_dim_text_mat");
		dimTextMat.color = vec4(0.18f, 0.21f, 0.27f, 1.0f);
		state.dimTextMat = dimTextMat.id;

		const MusicUiLayout l = computeMusicUiLayout();

		// 1. Play Button: Triangle
		{
			state.playButton =
				services.shapes().addUIShape({.shapeId = DefaultShapes::TriangleRotated,
											  .variables = {{DefaultShapes::TriangleRotated::PosX, 44.0f},
															{DefaultShapes::TriangleRotated::PosY, l.botY},
															{DefaultShapes::TriangleRotated::Width, 26.0f},
															{DefaultShapes::TriangleRotated::Height, 26.0f},
															{DefaultShapes::TriangleRotated::Angle, 1.5707963f}},
											  .material = playMat});
			auto& btn = registry.addComponent<ShapeButton>(state.playButton);
			btn.clickPadding = 8.0f;
			btn.modifierAmount = 3.0f;
			btn.parameterModifierMask.set(DefaultShapes::TriangleRotated::Width);
			btn.parameterModifierMask.set(DefaultShapes::TriangleRotated::Height);
		}

		// 2. Square Pause Button: Box
		{
			state.pauseButton = services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
															  .variables = {{DefaultShapes::Box::PosX, 92.0f},
																			{DefaultShapes::Box::PosY, l.botY},
																			{DefaultShapes::Box::SizeX, 13.0f},
																			{DefaultShapes::Box::SizeY, 13.0f}},
															  .material = pauseMat});
			auto& btn = registry.addComponent<ShapeButton>(state.pauseButton);
			btn.clickPadding = 8.0f;
			btn.modifierAmount = 3.0f;
			btn.parameterModifierMask.set(DefaultShapes::Box::SizeX);
			btn.parameterModifierMask.set(DefaultShapes::Box::SizeY);
		}

		// 3. Status Text
		{
			state.statusText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.statusText);
			t.position = vec3(136.0f, l.botY, 0.0f);
			auto& tr = registry.addComponent<UITextRenderer>(state.statusText);
			tr.text = "PAUSED";
			tr.material = textMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}

		// 4. Right Column: Clickable Surge & Duck TextButtons + 5-Bar Equalizers
		{
			const char* dynNames[State::NUM_DYNAMICS] = {"SURGE", "DUCK"};
			const Material2DHandle dynMats[State::NUM_DYNAMICS] = {state.surgeMat, state.duckMat};

			for (size_t i = 0; i < State::NUM_DYNAMICS; ++i)
			{
				state.dynamicLabels[i] = registry.createEntity();
				auto& lt = registry.addComponent<Transform>(state.dynamicLabels[i]);
				lt.position = vec3(l.rightLabelX, l.dynamicY[i], 0.0f);
				auto& ltr = registry.addComponent<UITextRenderer>(state.dynamicLabels[i]);
				ltr.text = dynNames[i];
				ltr.material = dynMats[i].id;
				ltr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
				ltr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
				auto& txtBtn = registry.addComponent<TextButton>(state.dynamicLabels[i]);
				txtBtn.clickPadding = 10.0f;
				txtBtn.baseMaterial = dynMats[i].id;
				txtBtn.hoverMaterial = state.textMat.id;
				txtBtn.pressedMaterial = state.textMat.id;
			}
		}

		// Surge & Duck 5-Bar Equalizer Gauges (track group 0, active fill group 1)
		auto createGaugeBars = [&](std::array<Entity, State::NUM_GAUGE_BARS>& trackBars,
								   std::array<Entity, State::NUM_GAUGE_BARS>& fillBars, Material2DHandle fillMat)
		{
			for (size_t i = 0; i < State::NUM_GAUGE_BARS; ++i)
			{
				trackBars[i] =
					services.shapes().addUIShape({.shapeId = DefaultShapes::RoundBox,
												  .variables = {{DefaultShapes::RoundBox::PosX, -1000.0f},
																{DefaultShapes::RoundBox::PosY, -1000.0f},
																{DefaultShapes::RoundBox::SizeX, l.gaugeHalfW},
																{DefaultShapes::RoundBox::SizeY, 5.0f},
																{DefaultShapes::RoundBox::Radius, 3.0f}},
												  .material = state.trackOffMat,
												  .group = 0});
				fillBars[i] = services.shapes().addUIShape({.shapeId = DefaultShapes::RoundBox,
															.variables = {{DefaultShapes::RoundBox::PosX, -1000.0f},
																		  {DefaultShapes::RoundBox::PosY, -1000.0f},
																		  {DefaultShapes::RoundBox::SizeX, 4.0f},
																		  {DefaultShapes::RoundBox::SizeY, 4.0f},
																		  {DefaultShapes::RoundBox::Radius, 2.0f}},
															.material = fillMat,
															.group = 1});
			}
		};
		createGaugeBars(state.surgeTrackBars, state.surgeFillBars, state.surgeMat);
		createGaugeBars(state.duckTrackBars, state.duckFillBars, state.duckMat);

		// 5. Left Column: Track Mixer Toggles (LEAD, BASS, PAD, DRUM)
		const char* trackNames[State::NUM_TRACKS] = {"LEAD", "BASS", "PAD", "DRUM"};
		const auto currentToggles = services.audio().music().getTrackToggles();
		const bool initStates[State::NUM_TRACKS] = {currentToggles.lead, currentToggles.bass, currentToggles.pad,
													currentToggles.drums};

		for (size_t i = 0; i < State::NUM_TRACKS; ++i)
		{
			state.trackVisualActive[i] = initStates[i] ? 1 : 0;
			Material2DHandle mat = initStates[i] ? state.trackOnMat : state.trackOffMat;
			state.trackToggles[i] =
				services.shapes().addUIShape({.shapeId = DefaultShapes::RoundBox,
											  .variables = {{DefaultShapes::RoundBox::PosX, l.leftPillX},
															{DefaultShapes::RoundBox::PosY, l.trackY[i]},
															{DefaultShapes::RoundBox::SizeX, l.pillHalfSize},
															{DefaultShapes::RoundBox::SizeY, l.pillHalfSize},
															{DefaultShapes::RoundBox::Radius, 4.0f}},
											  .material = mat});
			auto& tgl = registry.addComponent<ShapeToggle>(state.trackToggles[i]);
			tgl.active = initStates[i];
			tgl.clickPadding = 6.0f;
			tgl.modifierAmount = 2.5f;
			tgl.parameterModifierMask.set(DefaultShapes::RoundBox::SizeX);
			tgl.parameterModifierMask.set(DefaultShapes::RoundBox::SizeY);

			state.trackLabels[i] = registry.createEntity();
			auto& lt = registry.addComponent<Transform>(state.trackLabels[i]);
			lt.position = vec3(l.leftLabelX, l.trackY[i], 0.0f);
			auto& ltr = registry.addComponent<UITextRenderer>(state.trackLabels[i]);
			ltr.text = trackNames[i];
			ltr.material = initStates[i] ? state.textMat.id : state.dimTextMat.id;
			ltr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			ltr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
			auto& txtTgl = registry.addComponent<TextToggle>(state.trackLabels[i]);
			txtTgl.active = initStates[i];
			txtTgl.clickPadding = 8.0f;
			txtTgl.activeMaterial = state.textMat.id;
			txtTgl.inactiveMaterial = state.dimTextMat.id;
			txtTgl.hoverMaterial = state.trackOnMat.id;
		}

		applyMusicUiLayout(registry, state, services);
	}

	void setupVisualizerDotsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		for (int i = 0; i < 10; ++i)
		{
			auto ee = registry.createEntity();
			auto& t = registry.addComponent<Transform>(ee);
			t.position = vec3(400.0f, 400.0f, 0.0f);

			auto& ui = registry.addComponent<UIDot>(ee);
			ui.materialId = state.songMats[i % State::NUM_SONGS];

			state.uiPoints[i] = ee;
		}
	}

	void setupWorldSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 0.0f, 25.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);

		auto& floorMat = services.materials2D().createMaterial("main_floor");
		floorMat.color = ColorPalette::LightGray;

		Vec2Expr p = point();
		Vec2Expr repeatingPoint = SDF::repeat(p, var(1));
		// Vec2Expr translated = translate(repeatingPoint, Vec2Expr(sin(var(0) * time()), cos(var(0) * time())));
		Expr finalShape = SDF::sdCircle(repeatingPoint, var(2) * audioVolume());

		ShapeId shapeId = services.shapes().registerSDF(finalShape);

		auto backgroundShape = services.shapes().addShape({.shapeId = shapeId,
														   .variables = {{0.0f, 5.0f, 5.0f}},
														   .material = floorMat,
														   .combination = CombinationType::Addition});

		services.names().setName(backgroundShape, "bg");

		getState(registry).backgroundShape = backgroundShape;
	}

	void updateWorldVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		State& state = getState(registry);
		if (registry.isEntityValid(state.backgroundShape) && registry.hasComponent<Shape>(state.backgroundShape))
		{
			const auto& musicEngine = services.audio().music();
			const float surgeLvl = musicEngine.getSurgeLevel();
			const float duckLvl = musicEngine.getDuckingLevel();
			auto& shape = registry.getComponent<Shape>(state.backgroundShape);
			shape.parameters[2] = (std::max)(2.0f, 5.0f + surgeLvl * 2.8f - duckLvl * 2.2f);
		}
	}

	void toggleSelectionSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Number key shortcuts 1-4 to select shapes
		int keySelected = -1;
		if (services.input().getKeyDown(Input::Num1))
			keySelected = 0;
		else if (services.input().getKeyDown(Input::Num2))
			keySelected = 1;
		else if (services.input().getKeyDown(Input::Num3))
			keySelected = 2;
		else if (services.input().getKeyDown(Input::Num4))
			keySelected = 3;

		if (keySelected >= 0 && keySelected < static_cast<int>(State::NUM_SONGS))
		{
			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (registry.isEntityValid(state.songToggles[i]) &&
					registry.hasComponent<ShapeToggle>(state.songToggles[i]))
				{
					auto& toggle = registry.getComponent<ShapeToggle>(state.songToggles[i]);
					toggle.active = (static_cast<int>(i) == keySelected);
				}
			}
			state.selectedSongIndex = keySelected;
			state.confirmedSongIndex = keySelected;
			state.cursorSongIndex = keySelected;
			if (s_songs[keySelected])
			{
				state.paramSize = s_songs[keySelected]->getParameter(0);
				state.paramSpeed = s_songs[keySelected]->getParameter(1);
			}
			services.audio().playSound({0.03f, 500.0f + keySelected * 90.0f, false, vec3(0.0f), 1});
			updateCursorDot(registry, state);
			return;
		}

		// Mouse toggle selection
		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			if (registry.isEntityValid(state.songToggles[i]) &&
				registry.hasComponent<ShapeToggle>(state.songToggles[i]))
			{
				auto& toggle = registry.getComponent<ShapeToggle>(state.songToggles[i]);
				if (toggle.active && state.selectedSongIndex != static_cast<int>(i))
				{
					// Toggle i was selected! Make it the only active toggle (radio behavior)
					state.selectedSongIndex = static_cast<int>(i);
					state.confirmedSongIndex = static_cast<int>(i);
					state.cursorSongIndex = static_cast<int>(i);
					if (s_songs[i])
					{
						state.paramSize = s_songs[i]->getParameter(0);
						state.paramSpeed = s_songs[i]->getParameter(1);
					}
					for (size_t j = 0; j < State::NUM_SONGS; ++j)
					{
						if (j != i && registry.isEntityValid(state.songToggles[j]) &&
							registry.hasComponent<ShapeToggle>(state.songToggles[j]))
						{
							registry.getComponent<ShapeToggle>(state.songToggles[j]).active = false;
						}
					}
					services.audio().playSound({0.03f, 500.0f + i * 90.0f, false, vec3(0.0f), 1});
					updateCursorDot(registry, state);
					break;
				}
			}
		}

		// Gamepad controller support
		bool dpadLeft = services.input().getGamepadButtonDown(Input::GamepadButton::DpadLeft);
		bool dpadRight = services.input().getGamepadButtonDown(Input::GamepadButton::DpadRight);
		bool dpadUp = services.input().getGamepadButtonDown(Input::GamepadButton::DpadUp);
		bool dpadDown = services.input().getGamepadButtonDown(Input::GamepadButton::DpadDown);
		bool crossDown = services.input().getGamepadButtonDown(Input::GamepadButton::South);
		bool circleDown = services.input().getGamepadButtonDown(Input::GamepadButton::East);

		if (!state.selectionMode)
		{
			if (dpadLeft || dpadRight || dpadUp || dpadDown)
			{
				state.selectionMode = true;
				if (state.selectedSongIndex >= 0 && state.selectedSongIndex < static_cast<int>(State::NUM_SONGS))
				{
					state.cursorSongIndex = state.selectedSongIndex;
				}
				else
				{
					state.cursorSongIndex = 0;
				}

				if (dpadLeft)
				{
					state.cursorSongIndex = (state.cursorSongIndex - 1 + State::NUM_SONGS) % State::NUM_SONGS;
				}
				else if (dpadRight)
				{
					state.cursorSongIndex = (state.cursorSongIndex + 1) % State::NUM_SONGS;
				}

				services.audio().playSound({0.02f, 500.0f + state.cursorSongIndex * 90.0f, false, vec3(0.0f), 1});
				updateCursorDot(registry, state);
			}
		}
		else
		{
			if (circleDown)
			{
				// PlayStation circle cancels selection mode
				state.selectionMode = false;
				state.cursorSongIndex = (state.selectedSongIndex >= 0 ? state.selectedSongIndex : 0);
				services.audio().playSound({0.03f, 320.0f, false, vec3(0.0f), 1});
				updateCursorDot(registry, state);
			}
			else if (crossDown)
			{
				// PlayStation cross confirms selection
				state.selectedSongIndex = state.cursorSongIndex;
				state.confirmedSongIndex = state.cursorSongIndex;
				for (size_t i = 0; i < State::NUM_SONGS; ++i)
				{
					if (registry.isEntityValid(state.songToggles[i]) &&
						registry.hasComponent<ShapeToggle>(state.songToggles[i]))
					{
						registry.getComponent<ShapeToggle>(state.songToggles[i]).active =
							(static_cast<int>(i) == state.cursorSongIndex);
					}
				}
				if (s_songs[state.cursorSongIndex])
				{
					state.paramSize = s_songs[state.cursorSongIndex]->getParameter(0);
					state.paramSpeed = s_songs[state.cursorSongIndex]->getParameter(1);
				}
				services.audio().playSound({0.04f, 750.0f, false, vec3(0.0f), 1});
			}
			else if (dpadLeft)
			{
				// Left moves dot cursor only
				state.cursorSongIndex = (state.cursorSongIndex - 1 + State::NUM_SONGS) % State::NUM_SONGS;
				services.audio().playSound({0.02f, 500.0f + state.cursorSongIndex * 90.0f, false, vec3(0.0f), 1});
				updateCursorDot(registry, state);
			}
			else if (dpadRight)
			{
				// Right moves dot cursor only
				state.cursorSongIndex = (state.cursorSongIndex + 1) % State::NUM_SONGS;
				services.audio().playSound({0.02f, 500.0f + state.cursorSongIndex * 90.0f, false, vec3(0.0f), 1});
				updateCursorDot(registry, state);
			}
		}
	}

	void playbackControlSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		bool playClicked = false;
		bool pauseClicked = false;

		// Check Play button
		if (registry.isEntityValid(state.playButton) && registry.hasComponent<ShapeButton>(state.playButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.playButton);
			if (btn.state == ButtonState::Down)
			{
				playClicked = true;
			}
		}

		// Check Pause button
		if (registry.isEntityValid(state.pauseButton) && registry.hasComponent<ShapeButton>(state.pauseButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.pauseButton);
			if (btn.state == ButtonState::Down)
			{
				pauseClicked = true;
			}
		}

		// Keyboard Space shortcut toggles play/pause
		if (services.input().getKeyDown(Input::Space))
		{
			if (state.isPlaying)
			{
				pauseClicked = true;
			}
			else
			{
				playClicked = true;
			}
		}

		// Controller: Triangle plays selected song, Square stops it
		if (services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			playClicked = true;
		}

		if (services.input().getGamepadButtonDown(Input::GamepadButton::West))
		{
			pauseClicked = true;
		}

		// Handle Play Action: loads selected song into preview UI via setSong()
		if (playClicked)
		{
			if (state.selectedSongIndex >= 0 && state.selectedSongIndex < static_cast<int>(State::NUM_SONGS))
			{
				loadAndPlaySong(registry, services, state, state.selectedSongIndex);
				services.audio().playSound({0.03f, 750.0f, false, vec3(0.0f), 1});
			}
			else
			{
				// No song selected prompt
				services.audio().playSound({0.04f, 240.0f, false, vec3(0.0f), 1});
			}
		}

		// Handle Pause Action: calls setSong(nullptr) and resets all toggles
		if (pauseClicked)
		{
			services.audio().setSong(nullptr, false);
			services.audio().music().setTrackToggles({false, false, false, false});
			services.audio().music().setVolume(0.0f);

			// Reset all shape toggles
			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (registry.isEntityValid(state.songToggles[i]) &&
					registry.hasComponent<ShapeToggle>(state.songToggles[i]))
				{
					registry.getComponent<ShapeToggle>(state.songToggles[i]).active = false;
				}
			}

			state.selectedSongIndex = -1;
			state.confirmedSongIndex = -1;
			state.currentLoadedSongIndex = -1;
			state.isPlaying = false;
			services.audio().playSound({0.03f, 320.0f, false, vec3(0.0f), 1});
			updateCursorDot(registry, state);
		}
	}

	void dynamicControlsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		const float dt = services.time().deltaTime();
		const MusicUiLayout l = computeMusicUiLayout();
		auto& musicEngine = services.audio().music();
		auto currentSong = musicEngine.getCurrentSong();

		auto ensureSongPlaying = [&]()
		{
			if (!state.isPlaying || !currentSong)
			{
				int targetIdx =
					(state.selectedSongIndex >= 0 && state.selectedSongIndex < static_cast<int>(State::NUM_SONGS))
						? state.selectedSongIndex
						: 0;
				loadAndPlaySong(registry, services, state, targetIdx);
				currentSong = musicEngine.getCurrentSong();
			}
		};

		const float surgeLvl = musicEngine.getSurgeLevel();
		const float duckLvl = musicEngine.getDuckingLevel();

		// 1. Right Column: Surge & Duck TextButton Labels + Gauge Click (+ Gamepad R1=Surge, L1=Duck)
		const bool gamepadTriggers[State::NUM_DYNAMICS] = {
			services.input().getGamepadButtonDown(Input::GamepadButton::RightShoulder),
			services.input().getGamepadButtonDown(Input::GamepadButton::LeftShoulder)};

		const bool blockClickAvailable = !services.input().isUIClick() && Input::GetMouseButtonDown(Input::LeftClick);
		const float clickX = Input::GetMouseX();
		const float clickY = Input::GetMouseY();

		for (size_t i = 0; i < State::NUM_DYNAMICS; ++i)
		{
			state.dynamicFlashTimers[i] = (std::max)(0.0f, state.dynamicFlashTimers[i] - dt);

			bool triggered = isButtonEntityDown(registry, state.dynamicLabels[i]) || gamepadTriggers[i];

			if (!triggered && blockClickAvailable && clickX >= (l.gaugeLeftX - l.gaugeHalfW - 8.0f) &&
				clickX <= (l.rightLabelX + 8.0f) && clickY >= (l.gaugeBaseY[i] - 8.0f) &&
				clickY <= (l.gaugeBaseY[i] + l.gaugeMaxH + 6.0f))
			{
				Input::flagUIClick();
				triggered = true;
			}

			if (triggered)
			{
				state.dynamicFlashTimers[i] = 0.22f;
				ensureSongPlaying();
				if (i == State::DYN_SURGE)
				{
					services.audio().surge(0.6f);
				}
				else
				{
					services.audio().duck(0.6f);
				}
			}
		}

		// 2. Surge & Duck 5-Bar Equalizer Gauges
		const float audioVol = state.isPlaying ? services.audio().getAudioVolume() : 0.0f;
		const float timeSec = services.time().time();
		updateEqualizerGauge(registry, state.surgeTrackBars, state.surgeFillBars, l, l.gaugeBaseY[State::DYN_SURGE],
							 surgeLvl, audioVol, timeSec, true);
		updateEqualizerGauge(registry, state.duckTrackBars, state.duckFillBars, l, l.gaugeBaseY[State::DYN_DUCK],
							 duckLvl, audioVol, timeSec, false);

		// 3. Left Column: Bidirectional Track Mixer Toggles (LEAD, BASS, PAD, DRUM)
		auto toggles = musicEngine.getTrackToggles();
		bool* engineFields[State::NUM_TRACKS] = {&toggles.lead, &toggles.bass, &toggles.pad, &toggles.drums};
		bool userChangedTracks = false;

		for (size_t i = 0; i < State::NUM_TRACKS; ++i)
		{
			if (!registry.isEntityValid(state.trackToggles[i]) ||
				!registry.hasComponent<ShapeToggle>(state.trackToggles[i]))
				continue;

			auto& tgl = registry.getComponent<ShapeToggle>(state.trackToggles[i]);
			const bool hasTextToggle =
				registry.isEntityValid(state.trackLabels[i]) && registry.hasComponent<TextToggle>(state.trackLabels[i]);

			const bool prevVisual = (state.trackVisualActive[i] != 0);
			bool desired = prevVisual;
			if (tgl.active != prevVisual)
			{
				desired = tgl.active;
				userChangedTracks = true;
			}
			else if (hasTextToggle && registry.getComponent<TextToggle>(state.trackLabels[i]).active != prevVisual)
			{
				desired = registry.getComponent<TextToggle>(state.trackLabels[i]).active;
				userChangedTracks = true;
			}
			else if (*engineFields[i] != prevVisual)
			{
				desired = *engineFields[i];
			}

			tgl.active = desired;
			if (hasTextToggle)
			{
				registry.getComponent<TextToggle>(state.trackLabels[i]).active = desired;
			}
			*engineFields[i] = desired;
			state.trackVisualActive[i] = desired ? 1 : 0;

			if (registry.hasComponent<UIShape>(state.trackToggles[i]))
			{
				auto& s = registry.getComponent<UIShape>(state.trackToggles[i]);
				uint16_t targetMat = desired ? state.trackOnMat.id : state.trackOffMat.id;
				if (s.material != targetMat)
				{
					s.material = targetMat;
					registry.setComponentDirty(s);
				}
			}
			if (registry.isEntityValid(state.trackLabels[i]) &&
				registry.hasComponent<UITextRenderer>(state.trackLabels[i]))
			{
				auto& ltr = registry.getComponent<UITextRenderer>(state.trackLabels[i]);
				uint16_t targetMat = desired ? state.textMat.id : state.dimTextMat.id;
				if (ltr.material != targetMat)
				{
					ltr.material = targetMat;
					registry.setComponentDirty(ltr);
				}
			}
		}

		if (userChangedTracks)
		{
			musicEngine.setTrackToggles(toggles);
			services.audio().playSound({0.02f, 620.0f, false, vec3(0.0f), 1});
		}
	}

	void parameterControlSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		bool changed = false;

		// Up / Down arrow keys control parameter 0: overall size (volume)
		if (services.input().getKeyDown(Input::Up))
		{
			state.paramSize = (std::min)(44.0f, state.paramSize + 2.0f);
			changed = true;
		}
		if (services.input().getKeyDown(Input::Down))
		{
			state.paramSize = (std::max)(12.0f, state.paramSize - 2.0f);
			changed = true;
		}

		// Right / Left arrow keys control parameter 1: speed (tempo)
		if (services.input().getKeyDown(Input::Right))
		{
			state.paramSpeed = (std::min)(3.5f, state.paramSpeed + 0.15f);
			changed = true;
		}
		if (services.input().getKeyDown(Input::Left))
		{
			state.paramSpeed = (std::max)(0.2f, state.paramSpeed - 0.15f);
			changed = true;
		}

		if (changed && state.isPlaying && state.currentLoadedSongIndex >= 0)
		{
			services.audio().setSongParameter(0, state.paramSize);
			services.audio().setSongParameter(1, state.paramSpeed);
			services.audio().resampleShape();
		}
	}

	void statusTextSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (!registry.isEntityValid(state.statusText) || !registry.hasComponent<UITextRenderer>(state.statusText))
		{
			return;
		}

		const auto& musicEngine = services.audio().music();
		int code = 0;
		const char* label = "SELECT SHAPE";
		if (state.isPlaying && state.currentLoadedSongIndex >= 0)
		{
			if (musicEngine.getTrackPlayState(WeirdAudio::MusicTrack::Lead) == WeirdAudio::TrackPlayState::Dead)
			{
				code = 4;
				label = "FADING OUT";
			}
			else if (musicEngine.getSurgeLevel() > 0.15f)
			{
				code = 2;
				label = "SURGE ACTIVE";
			}
			else if (musicEngine.getDuckingLevel() > 0.15f)
			{
				code = 3;
				label = "DUCK ACTIVE";
			}
			else
			{
				code = 1;
				label = "PLAYING";
			}
		}
		else if (state.selectedSongIndex >= 0)
		{
			code = 5;
			label = "PRESS PLAY";
		}

		if (code != state.lastStatusCode)
		{
			state.lastStatusCode = code;
			auto& tr = registry.getComponent<UITextRenderer>(state.statusText);
			tr.text = label;
			registry.setComponentDirty(tr);
		}
	}

	void songLayoutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		int hash = (Display::width * 73856093) ^ Display::height;
		if (hash != state.lastResolutionHash)
		{
			state.lastResolutionHash = hash;
			const MusicUiLayout l = computeMusicUiLayout();
			glm::vec2 previewCenter(l.winW * 0.5f, l.winH * 0.5f);

			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (s_songs[i])
				{
					s_songs[i]->setCenter(previewCenter);
				}

				// Re-center toggles
				const float spacingX = 60.0f;
				const float totalWidth = spacingX * 3;
				const float startX = (l.winW * 0.5f) - (totalWidth * 0.5f);
				if (registry.isEntityValid(state.songToggles[i]) &&
					registry.hasComponent<UIShape>(state.songToggles[i]))
				{
					auto& shape = registry.getComponent<UIShape>(state.songToggles[i]);
					shape.parameters[2] = startX + i * spacingX;
					shape.parameters[3] = l.toggleY;
				}
			}

			applyMusicUiLayout(registry, state, services);

			if (state.isPlaying && state.currentLoadedSongIndex >= 0)
			{
				int idx = state.currentLoadedSongIndex;
				services.audio().setSong(s_songs[idx],
										 {.mode = SongVisualizationMode::UI, .material = state.songMats[idx]}, false);
				services.audio().setSongParameter(0, state.paramSize);
				services.audio().setSongParameter(1, state.paramSpeed);
				services.audio().resampleShape();
			}

			updateCursorDot(registry, state);
		}
	}

	void uiDotsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		const auto& musicEngine = services.audio().music();
		float surgeLvl = musicEngine.getSurgeLevel();
		float duckLvl = musicEngine.getDuckingLevel();
		float volume = state.isPlaying ? services.audio().getAudioVolume() : 0.05f;
		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		glm::vec2 center(winW * 0.5f, winH * 0.5f);
		float scaleFactor = (std::min)(winW, winH) / 800.0f;
		float radius = (62.0f + (volume * 34.0f) + (surgeLvl * 26.0f) - (duckLvl * 18.0f)) * scaleFactor;
		float speed = state.isPlaying ? (0.8f * state.paramSpeed * (1.0f + surgeLvl * 1.15f - duckLvl * 0.55f)) : 0.3f;
		float spacing = 2.0f * 3.14159f / static_cast<float>(state.uiPoints.size());

		for (size_t i = 0; i < state.uiPoints.size(); i++)
		{
			float angle = (services.time().time() * speed) + (static_cast<float>(i) * spacing);

			float px = center.x + std::cos(angle) * radius;
			float py = center.y + std::sin(angle) * radius;

			if (registry.isEntityValid(state.uiPoints[i]) && registry.hasComponent<Transform>(state.uiPoints[i]))
			{
				auto& t = registry.getComponent<Transform>(state.uiPoints[i]);
				t.position = vec3(px, py, 0.0f);
			}
		}
	}

} // namespace MusicSceneNamespace

void MusicScene::onImGuiRender(Registry& registry, ServiceProvider& services)
{
#ifndef WEIRD_DISABLE_IMGUI
	auto* pState = registry.getState<MusicSceneNamespace::State>();
	if (!pState)
		return;
	auto& state = *pState;
	auto& musicEngine = services.audio().music();
	auto currentSong = musicEngine.getCurrentSong();

	if (ImGui::CollapsingHeader("Procedural Song Director & Explorer", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent();

		// 1. Song Picker & Shape Parameters
		ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "Song & Shape Parameters:");
		const auto& defs = MusicShapes::getSongDefinitions();
		for (size_t i = 0; i < MusicSceneNamespace::State::NUM_SONGS; ++i)
		{
			if (i > 0)
				ImGui::SameLine();
			bool isCurrent = (state.currentLoadedSongIndex == static_cast<int>(i) && state.isPlaying);
			if (isCurrent)
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.65f, 0.35f, 1.0f));
			if (ImGui::Button(defs[i].title.c_str()))
			{
				state.selectedSongIndex = static_cast<int>(i);
				state.confirmedSongIndex = static_cast<int>(i);
				state.cursorSongIndex = static_cast<int>(i);
				state.currentLoadedSongIndex = static_cast<int>(i);
				state.isPlaying = true;

				float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
				float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
				glm::vec2 previewCenter(winW * 0.5f, winH * 0.5f);

				MusicSceneNamespace::s_songs[i]->setCenter(previewCenter);
				state.paramSize = MusicSceneNamespace::s_songs[i]->getParameter(0);
				state.paramSpeed = MusicSceneNamespace::s_songs[i]->getParameter(1);

				services.audio().setSong(MusicSceneNamespace::s_songs[i],
										 {.mode = SongVisualizationMode::UI, .material = state.songMats[i]}, false);
				services.audio().resampleShape();
				musicEngine.setTrackToggles({true, true, true, true});
				if (musicEngine.getVolume() < 0.1f)
					musicEngine.setVolume(0.65f);
			}
			if (isCurrent)
				ImGui::PopStyleColor();
		}

		bool paramChanged = false;
		if (ImGui::SliderFloat("Shape Size (Param 0 -> Fill/Volume)", &state.paramSize, 12.0f, 44.0f, "%.1f"))
		{
			paramChanged = true;
		}
		if (ImGui::SliderFloat("Shape Speed (Param 1 -> Motion/Tempo)", &state.paramSpeed, 0.2f, 3.5f, "%.2f"))
		{
			paramChanged = true;
		}
		if (paramChanged && state.isPlaying && state.currentLoadedSongIndex >= 0)
		{
			services.audio().setSongParameter(0, state.paramSize);
			services.audio().setSongParameter(1, state.paramSpeed);
			services.audio().resampleShape();
		}

		if (currentSong)
		{
			// 2. Live Sequencer Timeline (32-Bar Evolving A/B Soundtrack Form)
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Live Sequencer & 32-Bar Evolving Arrangement:");

			float playheadBeat = musicEngine.getPlayheadBeat();
			int currentStep = static_cast<int>(playheadBeat * 4.0f) % 512;
			int stepInBar = currentStep % 16;
			int barInSong = (currentStep / 16) % 32;
			int phraseInSong = barInSong / 4;
			int barInPhrase = barInSong % 4;
			int loopCycle = musicEngine.getLoopCycle();

			uint32_t arch = currentSong->getFingerprint().structuralHash % 3;
			const char* archNames[] = {"Groove-First (32-Bar A/B Story)",
									   "Ambient-First (Floating Intro -> Groove -> Bridge -> Breakdown)",
									   "Driving Minimalist (Persistent Groove, Alternating Layers)"};
			const char* phraseNames[8] = {"1: Intro Groove (No Lead)",
										  "2: Section A1 (Main Theme)",
										  "3: Section A2 (Theme Answer)",
										  "4: Groove Breather (Lead Rests)",
										  "5: Section B Bridge (vi-IV-I-V Lift)",
										  "6: Atmospheric Breakdown (No Drums)",
										  "7: Section A3 (Evolved Theme Return)",
										  "8: Outro / Cool-Down (No Lead)"};

			ImGui::Text("Arrangement Style: %s | Loop Cycle: #%d", archNames[arch], loopCycle + 1);
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f),
							   "Current Section: %s | Bar %d/32 (Phrase Bar %d/4) | Step %d/16",
							   phraseNames[phraseInSong], barInSong + 1, barInPhrase + 1, stepInBar + 1);

			// Visual 16-step beat grid
			for (int s = 0; s < 16; ++s)
			{
				if (s > 0)
					ImGui::SameLine();
				bool activeStep = (s == stepInBar && state.isPlaying);
				bool downbeat = (s % 4 == 0);
				if (activeStep)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.95f, 0.4f, 1.0f));
				else if (downbeat)
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.32f, 0.42f, 1.0f));
				else
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.16f, 0.20f, 1.0f));

				char stepLbl[16];
				std::snprintf(stepLbl, sizeof(stepLbl), "%d##step_%d", s + 1, s);
				ImGui::Button(stepLbl, ImVec2(22.0f, 20.0f));
				ImGui::PopStyleColor();
			}

			// 3. Musical Scale & Key Override (Experiment how different scales change the song's mood!)
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 1.0f), "Scale, Root Key & Tempo Overrides:");

			const char* scaleNames[] = {"Pentatonic Major (Happy/Open)", "Pentatonic Minor (Cool/Synthwave)",
										"Major (Bright/Do-Re-Mi)",		 "Natural Minor (Sad/Serious)",
										"Dorian (Jazzy/Groovy)",		 "Lydian (Dreamy/Sci-Fi)",
										"Chromatic (All 12 Notes)"};
			int scaleIdx = static_cast<int>(currentSong->getScale());
			if (ImGui::Combo("Musical Scale", &scaleIdx, scaleNames, IM_ARRAYSIZE(scaleNames)))
			{
				currentSong->setScale(static_cast<WeirdAudio::MusicalScale>(scaleIdx));
			}

			int rootMidi = currentSong->getRootMidi();
			std::string rootStr = WeirdAudio::SdfSong::midiToNoteString(rootMidi);
			if (ImGui::SliderInt("Root Note (Key)", &rootMidi, 48, 72, rootStr.c_str()))
			{
				currentSong->setRootMidi(rootMidi);
			}

			float baseTempo = currentSong->getTempo();
			if (ImGui::SliderFloat("Base Tempo (BPM)", &baseTempo, 50.0f, 180.0f, "%.1f BPM"))
			{
				currentSong->setTempo(baseTempo);
			}
			ImGui::Text("Effective Dynamic Tempo: %.1f BPM", musicEngine.getTempo());

			// 4. Track Mixer & Live Instrument Rack Override
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.85f, 1.0f), "Track Mixer & Live Instrument Timbre Override:");

			auto toggles = musicEngine.getTrackToggles();
			bool togglesChanged = false;
			if (ImGui::Checkbox("Lead (Melody)", &toggles.lead))
				togglesChanged = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("Bass (Groove)", &toggles.bass))
				togglesChanged = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("Pad (Chords)", &toggles.pad))
				togglesChanged = true;
			ImGui::SameLine();
			if (ImGui::Checkbox("Drums (Beat)", &toggles.drums))
				togglesChanged = true;
			if (togglesChanged)
			{
				musicEngine.setTrackToggles(toggles);
			}

			auto rack = musicEngine.getInstrumentRack();
			bool rackChanged = false;
			const char* waveNames[] = {"Soft Sine", "Bandlimited Saw", "Pulse / Square", "FM Pluck", "Wavefolder"};
			int leadW = static_cast<int>(rack.lead);
			if (ImGui::Combo("Lead Wave", &leadW, waveNames, 5))
			{
				rack.lead = static_cast<WeirdAudio::WaveType>(leadW);
				rackChanged = true;
			}
			int bassW = static_cast<int>(rack.bass);
			if (ImGui::Combo("Bass Wave", &bassW, waveNames, 5))
			{
				rack.bass = static_cast<WeirdAudio::WaveType>(bassW);
				rackChanged = true;
			}
			int padW = static_cast<int>(rack.pad);
			if (ImGui::Combo("Pad Wave", &padW, waveNames, 5))
			{
				rack.pad = static_cast<WeirdAudio::WaveType>(padW);
				rackChanged = true;
			}
			const char* kitNames[] = {"808 Electronic", "Acoustic Punch", "Industrial 909"};
			if (ImGui::Combo("Drum Kit", &rack.drumKit, kitNames, 3))
			{
				rackChanged = true;
			}
			if (rackChanged)
			{
				musicEngine.setInstrumentRack(rack);
			}

			// 5. Dynamic Song Controls (Surge & Duck)
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Dynamic Song Controls:");
			if (ImGui::Button("Trigger Surge (+Energy/Climax)"))
			{
				services.audio().surge(0.6f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Forces all paused tracks ON, adds 4-on-the-floor kick, opens filters, and boosts "
								  "tempo!");
			}
			ImGui::SameLine();
			if (ImGui::Button("Trigger Duck (+Danger/Stealth)"))
			{
				services.audio().duck(0.6f);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Mutes Lead melody, slows tempo, darkens filters, and switches drums to a tense "
								  "heartbeat!");
			}

			ImGui::ProgressBar(musicEngine.getSurgeLevel(), ImVec2(180.0f, 0.0f), "Surge");
			ImGui::SameLine();
			ImGui::ProgressBar(musicEngine.getDuckingLevel(), ImVec2(180.0f, 0.0f), "Ducking");

			// 6. Shape-to-Music Visual Traits Inspector
			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::TreeNodeEx("How the SDF Shape Drives This Song (4 Visual Traits)",
								  ImGuiTreeNodeFlags_DefaultOpen))
			{
				const auto& sp = musicEngine.getShapeParameters();
				const auto traits = musicEngine.getVisualTraits();
				ImGui::BulletText("1. Size / Mass (%.2f)      -> Bass Weight (%.2f) & Harmony Richness (%.2f)",
								  traits.mass, sp.bassWeight, sp.harmonyRichness);
				ImGui::BulletText("2. Motion Speed (%.2f)     -> Tempo Factor (%.2fx) & Percussion Energy (%.2f)",
								  traits.motion, sp.tempoFactor, sp.percEnergy);
				ImGui::BulletText(
					"3. Spikiness / Sharp (%.2f)-> Brightness (%.2f), Melody Density (%.2f) & Synth Timbre",
					traits.spikiness, sp.brightness, sp.melodyDensity);
				ImGui::BulletText("4. Hollow (%.2f)/Asym (%.2f)-> Syncopation (%.2f), Variation (%.2f) & Scale Mood",
								  traits.hollowness, traits.asymmetry, sp.syncopation, sp.variation);
				ImGui::TreePop();
			}
		}
		else
		{
			ImGui::TextDisabled(
				"Select one of the 4 songs above (or press 1-4 then Space) to inspect & tweak it live!");
		}

		ImGui::Unindent();
	}
#endif
}