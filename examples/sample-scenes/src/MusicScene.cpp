#include "MusicScene.h"
#include "MusicShapes.h"

#include <cstdio>

using namespace WeirdEngine;

namespace MusicSceneNamespace
{
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
		state.lastResolutionHash = Display::width + Display::height;

		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			Material2D& mat = services.materials2D().createMaterial("music_mat_" + std::to_string(i));
			mat.color = ColorPalette::Default[8 + i % ColorPalette::Default.size()];
			state.songMats[i] = mat.id;
		}

		// 3. Instantiate the 4 songs using shared MusicShapes definitions
		for (size_t i = 0; i < State::NUM_SONGS; ++i)
		{
			state.songs[i] = MusicShapes::createSong(i, previewCenter);
		}

		auto activeSong = services.audio().music().getCurrentSong();
		int activeIdx = -1;
		if (activeSong && services.audio().music().getVolume() > 0.0f)
		{
			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (state.songs[i]->getName() == activeSong->getName())
				{
					state.songs[i] = activeSong;
					activeIdx = static_cast<int>(i);
					break;
				}
			}
		}

		if (activeIdx >= 0)
		{
			state.selectedSongIndex = activeIdx;
			state.currentLoadedSongIndex = activeIdx;
			state.isPlaying = true;
			state.paramSize = activeSong->getParameter(0);
			state.paramSpeed = activeSong->getParameter(1);
			state.songs[activeIdx]->setCenter(previewCenter);
			services.audio().setSong(
				state.songs[activeIdx],
				SongVisualizationOptions{.mode = SongVisualizationMode::UI, .material = state.songMats[activeIdx]},
				false);
		}
		else
		{
			services.audio().setSong(nullptr, false);
			services.audio().music().setTrackToggles({false, false, false, false});
			services.audio().music().setVolume(0.0f);
		}
	}

	void setupTogglesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		const float spacingX = 60.0f;
		const float totalWidth = spacingX * 3;
		const float startX = (winW * 0.5f) - (totalWidth * 0.5f);
		const float toggleY = winH - 55.0f;

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
				 .variables = {{0, 16.0f}, {1, 1.0f}, {2, startX + static_cast<float>(i) * spacingX}, {3, toggleY}},
				 .material = state.songMats[i]});
			auto& toggle = registry.addComponent<ShapeToggle>(state.songToggles[i]);
			toggle.active = (state.selectedSongIndex == static_cast<int>(i));
			toggle.clickPadding = 6.0f;
			toggle.modifierAmount = 4.0f;
			toggle.parameterModifierMask.set(0);
		}
	}

	void setupControlsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		Material2D& playMat = services.materials2D().createMaterial("music_play_mat");
		playMat.color = vec4(0.2f, 0.95f, 0.3f, 0.95f);

		Material2D& pauseMat = services.materials2D().createMaterial("music_pause_mat");
		pauseMat.color = vec4(0.95f, 0.2f, 0.25f, 0.95f);

		Material2D& textMat = services.materials2D().createMaterial("music_text_mat");
		textMat.color = ColorPalette::White;

		// 1. Play Button: Triangle ▶
		{
			state.playButton =
				services.shapes().addUIShape({.shapeId = DefaultShapes::TriangleRotated,
											  .variables = {{DefaultShapes::TriangleRotated::PosX, 55.0f},
															{DefaultShapes::TriangleRotated::PosY, 55.0f},
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

		// 2. Square Pause Button: Box ⏹
		{
			state.pauseButton = services.shapes().addUIShape({.shapeId = DefaultShapes::Box,
															  .variables = {{DefaultShapes::Box::PosX, 105.0f},
																			{DefaultShapes::Box::PosY, 55.0f},
																			{DefaultShapes::Box::SizeX, 13.0f},
																			{DefaultShapes::Box::SizeY, 13.0f}},
															  .material = pauseMat});
			auto& btn = registry.addComponent<ShapeButton>(state.pauseButton);
			btn.clickPadding = 8.0f;
			btn.modifierAmount = 3.0f;
			btn.parameterModifierMask.set(DefaultShapes::Box::SizeX);
			btn.parameterModifierMask.set(DefaultShapes::Box::SizeY);
		}

		// 3. UI Texts
		{
			state.statusText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.statusText);
			t.position = vec3(160.0f, 55.0f, 0.0f);
			auto& tr = registry.addComponent<UITextRenderer>(state.statusText);
			tr.text = "PAUSED";
			tr.material = textMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}
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

			state.uiPoints.push_back(ee);
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

		services.tags().tag(backgroundShape, "bg");

		getState(registry).backgroundShape = backgroundShape;
	}

	void updateWorldVisualsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		// Shape& shape = registry.getComponent<Shape>(getState(registry).backgroundShape);
		// registry.setComponentDirty(shape);
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
			if (state.songs[keySelected])
			{
				state.paramSize = state.songs[keySelected]->getParameter(0);
				state.paramSpeed = state.songs[keySelected]->getParameter(1);
			}
			services.audio().playSound({0.03f, 500.0f + keySelected * 90.0f, false, vec3(0.0f), 1});
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
					if (state.songs[i])
					{
						state.paramSize = state.songs[i]->getParameter(0);
						state.paramSpeed = state.songs[i]->getParameter(1);
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
					break;
				}
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

		// Handle Play Action: loads selected song into preview UI via setSong()
		if (playClicked)
		{
			if (state.selectedSongIndex >= 0 && state.selectedSongIndex < static_cast<int>(State::NUM_SONGS))
			{
				int idx = state.selectedSongIndex;
				state.currentLoadedSongIndex = idx;
				state.isPlaying = true;

				float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
				float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
				glm::vec2 previewCenter(winW * 0.5f, winH * 0.5f);

				state.songs[idx]->setCenter(previewCenter);
				state.songs[idx]->setParameter(0, state.paramSize);
				state.songs[idx]->setParameter(1, state.paramSpeed);

				services.audio().setSong(state.songs[idx],
										 {.mode = SongVisualizationMode::UI, .material = state.songMats[idx]}, false);
				services.audio().setSongParameter(0, state.paramSize);
				services.audio().setSongParameter(1, state.paramSpeed);
				services.audio().resampleShape();

				services.audio().music().setTrackToggles({true, true, true, true});
				services.audio().music().setVolume(0.65f);
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
			state.currentLoadedSongIndex = -1;
			state.isPlaying = false;
			services.audio().playSound({0.03f, 320.0f, false, vec3(0.0f), 1});
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

		// 1. Status Text
		if (registry.isEntityValid(state.statusText) && registry.hasComponent<UITextRenderer>(state.statusText))
		{
			auto& tr = registry.getComponent<UITextRenderer>(state.statusText);
			if (state.isPlaying && state.currentLoadedSongIndex >= 0)
			{
				tr.text = "PLAYING";
			}
			else if (state.selectedSongIndex >= 0)
			{
				tr.text = "PRESS PLAY";
			}
			else
			{
				tr.text = "SELECT SHAPE";
			}
			registry.setComponentDirty(tr);
		}
	}

	void songLayoutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		int hash = Display::width + Display::height;
		if (hash != state.lastResolutionHash)
		{
			state.lastResolutionHash = hash;
			float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
			float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
			glm::vec2 previewCenter(winW * 0.5f, winH * 0.5f);

			for (size_t i = 0; i < State::NUM_SONGS; ++i)
			{
				if (state.songs[i])
				{
					state.songs[i]->setCenter(previewCenter);
				}

				// Re-center toggles
				const float spacingX = 60.0f;
				const float totalWidth = spacingX * 3;
				const float startX = (winW * 0.5f) - (totalWidth * 0.5f);
				const float toggleY = winH - 55.0f;
				if (registry.isEntityValid(state.songToggles[i]) &&
					registry.hasComponent<UIShape>(state.songToggles[i]))
				{
					auto& shape = registry.getComponent<UIShape>(state.songToggles[i]);
					shape.parameters[2] = startX + i * spacingX;
					shape.parameters[3] = toggleY;
				}
			}

			if (state.isPlaying && state.currentLoadedSongIndex >= 0)
			{
				int idx = state.currentLoadedSongIndex;
				services.audio().setSong(state.songs[idx],
										 {.mode = SongVisualizationMode::UI, .material = state.songMats[idx]}, false);
				services.audio().setSongParameter(0, state.paramSize);
				services.audio().setSongParameter(1, state.paramSpeed);
				services.audio().resampleShape();
			}
		}
	}

	void uiDotsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		float volume = state.isPlaying ? services.audio().getAudioVolume() : 0.05f;
		float winW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float winH = static_cast<float>(Display::height > 0 ? Display::height : 800);
		glm::vec2 center(winW * 0.5f, winH * 0.5f);
		float radius = 60.0f + (volume * 40.0f);
		float speed = state.isPlaying ? (0.8f * state.paramSpeed) : 0.3f;
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