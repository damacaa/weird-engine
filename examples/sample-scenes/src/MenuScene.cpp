#include "MenuScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <string>

using namespace WeirdEngine;
using namespace WeirdEngine::SDF;
using namespace WeirdEngine::WeirdAudio;

namespace MenuSceneNamespace
{
	namespace
	{
		bool s_menuPositionSaved = false;
		int s_savedCategory = 0;
		int s_savedItem = 0;
		int s_savedCategoryItems[State::CATEGORY_COUNT] = {0, 0, 0};
		int s_savedTheme = 0;
	} // namespace
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "MenuScene State is missing: stateInitSystem must run first");
		return *state;
	}

	// Procedural SDF Shape generator for Song 1 (Synthwave Star)
	static Expr getSongShape01(Vec2Expr p)
	{
		Expr star = sdStar(p, Expr(var(0)), Expr(var(0)) * 0.2f, 12.0f, Expr(var(1)) * 5.0f);
		Expr eclipseX = sin(Expr(var(1)) * time() * 0.7f) * (Expr(var(0)) * 1.0f);
		Vec2Expr moonCenter = {eclipseX, Expr(0.0f)};
		Expr moon = sdCircle(p - moonCenter, Expr(var(0)) * 0.42f);
		return sdfSubtract(star, moon);
	}

	// Procedural SDF Shape generator for Song 2 (Ambient Wave Bubble)
	static Expr getSongShape02(Vec2Expr p)
	{
		Expr wave = sdSineWave(p, Expr(var(0)) * 0.22f, 0.08f, Expr(var(1)) * 1.5f, 0.0f);
		Expr bubbleY = sin(Expr(var(1)) * time() * 2.0f) * (Expr(var(0)) * 0.25f);
		Vec2Expr bubbleCenter = {Expr(0.0f), bubbleY};
		Expr bubble = sdCircle(p - bubbleCenter, Expr(var(0)) * 0.45f);
		return sdfSmoothUnion(wave, bubble, Expr(var(0)) * 0.15f);
	}

	// Procedural SDF Shape generator for Song 3 (Velocity Star Shuriken)
	static Expr getSongShape03(Vec2Expr p)
	{
		Expr theta = Expr(var(1)) * time() * 1.6f;
		Vec2Expr rotP = {p.x * cos(theta) - p.y * sin(theta), p.x * sin(theta) + p.y * cos(theta)};
		Expr shuriken = sdStar(rotP, Expr(var(0)) * 0.75f, Expr(var(0)) * 0.42f, 4.0f, 0.0f);
		Expr centerHole = sdBox(rotate(p, theta * -1.0f), Vec2Expr(var(0) * 0.5f, var(0) * 0.5f));
		return sdfSubtract(shuriken, centerHole);
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		auto& state = registry.emplaceState<State>();

		// Initialize 3 Categories: GAME, MUSIC, SETTINGS
		state.categories = {
			MenuCategory{"GAME",
						 {
							 {"SHAPES", "2D Signed Distance Field shapes showcase", "launch_scene", "shapes"},
							 {"ROPE", "Verlet physics rope simulation", "launch_scene", "rope"},
							 {"WALK", "Character platformer and terrain demo", "launch_scene", "walk"},
							 {"LIFE", "Game of Life cellular automata", "launch_scene", "life"},
							 {"CURSOR", "Mouse cursor collision dynamics", "launch_scene", "cursor-collision"},
							 {"IMAGE", "Image Signed Distance Field processing", "launch_scene", "image"},
							 {"MUSIC DEMO", "Original Music Scene visualizer", "launch_scene", "music"},
						 }},
			MenuCategory{"MUSIC",
						 {
							 {"TRACK 01", "Midnight Pulse (Synthwave Beat)", "play_song", "", 0},
							 {"TRACK 02", "Silica Dreams (Ambient Chill)", "play_song", "", 1},
							 {"TRACK 03", "Velocity (Electronic Melody)", "play_song", "", 2},
							 {"STOP MUSIC", "Silence background audio playback", "stop_song", "", -1},
						 }},
			MenuCategory{"SETTINGS",
						 {
							 {"THEME", "Cycle background wave color palette", "cycle_theme", ""},
							 {"SYSTEM", "WeirdEngine OS Version 3.80 - Free Space: 112 GB", "system_info", ""},
						 }}};

		if (s_menuPositionSaved)
		{
			state.selectedCategory = std::clamp(s_savedCategory, 0, static_cast<int>(state.categories.size()) - 1);
			for (size_t i = 0; i < state.categories.size() && i < State::CATEGORY_COUNT; ++i)
			{
				int maxItem = static_cast<int>(state.categories[i].items.size()) - 1;
				state.categories[i].selectedItem = std::clamp(s_savedCategoryItems[i], 0, maxItem);
			}
			int curItemsMax = static_cast<int>(state.categories[state.selectedCategory].items.size()) - 1;
			state.selectedItem = std::clamp(state.categories[state.selectedCategory].selectedItem, 0, curItemsMax);
			state.smoothCategory = static_cast<float>(state.selectedCategory);
			state.smoothItem = static_cast<float>(state.selectedItem);
			state.currentTheme = s_savedTheme;
			state.categoryTitleTimer = (state.selectedItem != 0) ? 1.0f : 0.0f;
		}
		else
		{
			state.selectedCategory = 0; // Default to GAME
			state.selectedItem = 0;
			state.smoothCategory = 0.0f;
			state.smoothItem = 0.0f;
			state.currentTheme = 0;
		}

		// Instantiate 3 procedural songs using WeirdEngine's SdfSong music synthesizer
		auto song1 = WeirdAudio::SdfSong::create("Midnight Pulse", getSongShape01(point()), 120.0f,
												 WeirdAudio::MusicalScale::PentatonicMinor, 57);
		song1->setParameter(0, 26.0f);
		song1->setParameter(1, 1.0f);

		auto song2 = WeirdAudio::SdfSong::create("Silica Dreams", getSongShape02(point()), 92.0f,
												 WeirdAudio::MusicalScale::PentatonicMajor, 60);
		song2->setParameter(0, 24.0f);
		song2->setParameter(1, 0.8f);

		auto song3 = WeirdAudio::SdfSong::create("Velocity", getSongShape03(point()), 138.0f,
												 WeirdAudio::MusicalScale::Dorian, 62);
		song3->setParameter(0, 22.0f);
		song3->setParameter(1, 1.2f);

		state.songs = {song1, song2, song3};
		state.currentPlayingSong = -1;
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(true);
		services.debug().setDebugFly(false);

		// Camera looking at XY plane from Z=25, aligned with restored category parallax
		float catOffset = state.smoothCategory - 1.0f; // -1: GAME, 0: MUSIC, +1: SETTINGS
		float parallaxX = catOffset * 3.5f;

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(parallaxX, 0.0f, 25.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);

		state.lastResolutionHash = Display::width + Display::height;
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		// Background World Materials
		services.materials2D().createMaterial("bg_wave_primary");
		services.materials2D().createMaterial("bg_wave_secondary");
		services.materials2D().createMaterial("bg_wave_accent");
		services.materials2D().createMaterial("bg_wave_bright");
		services.materials2D().createMaterial("bg_particle");

		// UI Materials
		Material2D& uiWhite = services.materials2D().createMaterial("ui_white");
		uiWhite.color = vec4(1.0f, 1.0f, 1.0f, 0.98f);

		Material2D& uiDimmed = services.materials2D().createMaterial("ui_dimmed");
		uiDimmed.color = vec4(0.72f, 0.78f, 0.88f, 0.38f);

		Material2D& uiCyan = services.materials2D().createMaterial("ui_cyan");
		uiCyan.color = vec4(0.25f, 0.75f, 1.0f, 0.95f);

		Material2D& uiToast = services.materials2D().createMaterial("ui_toast");
		uiToast.color = vec4(1.0f, 0.84f, 0.28f, 0.95f);

		// Apply active theme (restored or default Classic Midnight Grid)
		State& state = getState(registry);
		applyTheme(registry, services, state.currentTheme);
	}

	void setupWorldWaveSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Register custom wave ribbon SDF
		Expr wave = (point().y - var(3)) - var(0) * sin(var(1) * point().x + var(2) * time() + var(5));
		Expr waveRibbon = sdfOnion(wave, var(4));
		state.waveRibbonSdfId = services.shapes().registerSDF(waveRibbon);

		auto& matPrim = services.materials2D().get("bg_wave_primary");
		auto& matSec = services.materials2D().get("bg_wave_secondary");
		auto& matAcc = services.materials2D().get("bg_wave_accent");
		auto& matBrt = services.materials2D().get("bg_wave_bright");

		// Ribbon 0: Main flowing foreground ribbon
		state.waveRibbons[0] = services.shapes().addShape({
			.shapeId = state.waveRibbonSdfId,
			.variables = {2.6f, 0.11f, 0.85f, -2.2f, 0.55f, 0.0f},
			.material = matPrim,
			.combination = CombinationType::Addition,
			.hasCollision = false,
			.group = 0,
		});

		// Ribbon 1: Harmonic secondary wave ribbon
		state.waveRibbons[1] = services.shapes().addShape({
			.shapeId = state.waveRibbonSdfId,
			.variables = {2.1f, 0.15f, 0.62f, -1.4f, 0.42f, 1.8f},
			.material = matSec,
			.combination = CombinationType::Addition,
			.hasCollision = false,
			.group = 1,
		});

		// Ribbon 2: Deep lower ambient wave ribbon
		state.waveRibbons[2] = services.shapes().addShape({
			.shapeId = state.waveRibbonSdfId,
			.variables = {3.6f, 0.07f, 0.42f, -4.2f, 0.85f, 3.2f},
			.material = matAcc,
			.combination = CombinationType::Addition,
			.hasCollision = false,
			.group = 2,
		});

		// Ribbon 3: Delicate high flutter wave ribbon
		state.waveRibbons[3] = services.shapes().addShape({
			.shapeId = state.waveRibbonSdfId,
			.variables = {1.1f, 0.22f, 1.35f, -2.6f, 0.22f, 0.8f},
			.material = matBrt,
			.combination = CombinationType::Addition,
			.hasCollision = false,
			.group = 3,
		});

		// Ambient drifting particles in the 2D raymarching world
		auto& matParticle = services.materials2D().get("bg_particle");
		state.particles.resize(8);

		float initialX[8] = {-28.0f, -18.0f, -8.0f, 2.0f, 12.0f, 22.0f, 32.0f, -22.0f};
		float initialY[8] = {-3.5f, -1.5f, -4.8f, -2.0f, -5.2f, -1.8f, -3.8f, -4.2f};
		float speedX[8] = {1.2f, 0.8f, 1.5f, 0.9f, 1.1f, 0.7f, 1.4f, 1.0f};
		float freqY[8] = {1.2f, 1.8f, 0.9f, 1.5f, 1.1f, 2.0f, 1.3f, 1.6f};
		float ampY[8] = {0.8f, 0.6f, 1.1f, 0.7f, 0.9f, 0.5f, 1.0f, 0.75f};
		float radii[8] = {0.45f, 0.35f, 0.55f, 0.40f, 0.50f, 0.30f, 0.60f, 0.42f};

		for (size_t i = 0; i < 8; ++i)
		{
			state.particles[i].baseX = initialX[i];
			state.particles[i].baseY = initialY[i];
			state.particles[i].speedX = speedX[i];
			state.particles[i].freqY = freqY[i];
			state.particles[i].ampY = ampY[i];
			state.particles[i].phase = static_cast<float>(i) * 1.1f;
			state.particles[i].baseRadius = radii[i];

			state.particles[i].entity = services.shapes().addShape({
				.shapeId = DefaultShapes::Circle,
				.variables = {initialX[i], initialY[i], radii[i]},
				.material = matParticle,
				.combination = CombinationType::Addition,
				.hasCollision = false,
				.group = Shape::GLOBAL_GROUP,
			});
		}
	}

	void setupCategoryUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& uiDimmed = services.materials2D().get("ui_dimmed");
		auto& uiCyan = services.materials2D().get("ui_cyan");
		auto& uiWhite = services.materials2D().get("ui_white");

		// Category outer icon shapes: All use CircleLine for 100% identical outer size
		uint16_t outerShapes[State::CATEGORY_COUNT] = {
			DefaultShapes::CircleLine, // GAME
			DefaultShapes::CircleLine, // MUSIC
			DefaultShapes::CircleLine, // SETTINGS
		};

		// Category inner icon shapes:
		uint16_t innerShapes[State::CATEGORY_COUNT] = {
			DefaultShapes::Box,		   // GAME (console / gamepad body)
			DefaultShapes::Circle,	   // MUSIC (optical disc hub / spindle)
			DefaultShapes::BoxRotated, // SETTINGS (diamond calibration dial)
		};

		for (size_t i = 0; i < State::CATEGORY_COUNT; ++i)
		{
			// Outer icon (uniform CircleLine)
			state.categoryOuterIcons[i] = services.shapes().addUIShape({
				.shapeId = outerShapes[i],
				.variables = {0.0f, 0.0f, 15.0f, 1.6f},
				.material = uiDimmed,
			});

			// Inner icon
			state.categoryInnerIcons[i] = services.shapes().addUIShape({
				.shapeId = innerShapes[i],
				.variables = {0.0f, 0.0f, 5.0f, 5.0f, 0.785398f},
				.material = uiDimmed,
			});
		}

		// Active Category Focus Halo Ring (pulsing ring centered on active category)
		{
			state.categoryFocusRing = services.shapes().addUIShape({
				.shapeId = DefaultShapes::CircleLine,
				.variables = {0.0f, 0.0f, 25.0f, 1.8f},
				.material = uiCyan,
			});
		}

		// Active Category Title Text (shown above the category icon when selectedItem == 0)
		{
			state.activeCategoryLabel = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.activeCategoryLabel);
			t.position = vec3(0.0f, 0.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.activeCategoryLabel);
			tr.text = state.categories[state.selectedCategory].name;
			tr.material = uiWhite.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}
	}

	void setupItemUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& uiWhite = services.materials2D().get("ui_white");
		auto& uiDimmed = services.materials2D().get("ui_dimmed");
		auto& uiToast = services.materials2D().get("ui_toast");

		// Pool of vertical item slots (-4 to +4 relative to active slot)
		for (int i = 0; i < State::VISIBLE_ITEM_SLOTS; ++i)
		{
			// Slot item label
			state.itemLabels[i] = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.itemLabels[i]);
			t.position = vec3(-1000.0f, -1000.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.itemLabels[i]);
			tr.text = "";
			tr.material = (i == 4) ? uiWhite.id : uiDimmed.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}

		// Notification Toast
		{
			state.toastLabel = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.toastLabel);
			t.position = vec3(0.0f, 0.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.toastLabel);
			tr.text = "";
			tr.material = uiToast.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}
	}

	void setupStatusUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& uiWhite = services.materials2D().get("ui_white");

		// Header: Minimal Clock text in top-right
		{
			state.clockLabel = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.clockLabel);
			t.position =
				vec3(static_cast<float>(Display::width) - 40.0f, static_cast<float>(Display::height) - 40.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.clockLabel);
			tr.text = "12:00";
			tr.material = uiWhite.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}
	}

	void applyTheme(Registry& registry, ServiceProvider& services, int themeIndex)
	{
		State& state = getState(registry);
		state.currentTheme = (themeIndex % 6 + 6) % 6;

		auto& bg = services.render().getBackground();
		bg.type = BackgroundType::Grid;
		bg.scale = 1.0f;
		bg.intensity = 1.0f;
		bg.parallax = 0.5f;

		auto& matPrim = services.materials2D().get("bg_wave_primary");
		auto& matSec = services.materials2D().get("bg_wave_secondary");
		auto& matAcc = services.materials2D().get("bg_wave_accent");
		auto& matBrt = services.materials2D().get("bg_wave_bright");
		auto& matPart = services.materials2D().get("bg_particle");
		auto& matCyan = services.materials2D().get("ui_cyan");

		switch (state.currentTheme)
		{
			case 0: // Classic Midnight Grid
				bg.primaryColor = vec4(0.03f, 0.05f, 0.10f, 1.0f);
				bg.secondaryColor = vec4(0.10f, 0.16f, 0.28f, 1.0f);
				matPrim.color = vec4(0.24f, 0.68f, 1.0f, 0.85f);
				matSec.color = vec4(0.12f, 0.44f, 0.85f, 0.65f);
				matAcc.color = vec4(0.35f, 0.20f, 0.72f, 0.45f);
				matBrt.color = vec4(0.75f, 0.92f, 1.0f, 0.95f);
				matPart.color = vec4(0.70f, 0.90f, 1.0f, 0.40f);
				matCyan.color = vec4(0.25f, 0.75f, 1.0f, 0.95f);
				state.toastMessage = "THEME: MIDNIGHT GRID";
				break;

			case 1: // Obsidian Onyx Grid
				bg.primaryColor = vec4(0.02f, 0.02f, 0.03f, 1.0f);
				bg.secondaryColor = vec4(0.12f, 0.14f, 0.18f, 1.0f);
				matPrim.color = vec4(0.85f, 0.88f, 0.95f, 0.82f);
				matSec.color = vec4(0.50f, 0.55f, 0.68f, 0.62f);
				matAcc.color = vec4(0.28f, 0.32f, 0.42f, 0.42f);
				matBrt.color = vec4(1.0f, 1.0f, 1.0f, 0.95f);
				matPart.color = vec4(0.90f, 0.92f, 1.0f, 0.35f);
				matCyan.color = vec4(0.90f, 0.90f, 0.95f, 0.95f);
				state.toastMessage = "THEME: OBSIDIAN GRID";
				break;

			case 2: // Crimson Silk Grid
				bg.primaryColor = vec4(0.08f, 0.02f, 0.03f, 1.0f);
				bg.secondaryColor = vec4(0.24f, 0.06f, 0.09f, 1.0f);
				matPrim.color = vec4(1.0f, 0.25f, 0.38f, 0.85f);
				matSec.color = vec4(0.85f, 0.15f, 0.30f, 0.65f);
				matAcc.color = vec4(0.52f, 0.10f, 0.22f, 0.45f);
				matBrt.color = vec4(1.0f, 0.75f, 0.82f, 0.95f);
				matPart.color = vec4(1.0f, 0.70f, 0.75f, 0.40f);
				matCyan.color = vec4(1.0f, 0.35f, 0.45f, 0.95f);
				state.toastMessage = "THEME: CRIMSON GRID";
				break;

			case 3: // Emerald Aurora Grid
				bg.primaryColor = vec4(0.02f, 0.06f, 0.03f, 1.0f);
				bg.secondaryColor = vec4(0.06f, 0.22f, 0.11f, 1.0f);
				matPrim.color = vec4(0.20f, 0.95f, 0.55f, 0.85f);
				matSec.color = vec4(0.10f, 0.70f, 0.58f, 0.65f);
				matAcc.color = vec4(0.08f, 0.42f, 0.26f, 0.45f);
				matBrt.color = vec4(0.70f, 1.0f, 0.85f, 0.95f);
				matPart.color = vec4(0.65f, 1.0f, 0.80f, 0.40f);
				matCyan.color = vec4(0.30f, 0.95f, 0.60f, 0.95f);
				state.toastMessage = "THEME: EMERALD GRID";
				break;

			case 4: // Amethyst Purple Grid
				bg.primaryColor = vec4(0.05f, 0.02f, 0.08f, 1.0f);
				bg.secondaryColor = vec4(0.20f, 0.06f, 0.28f, 1.0f);
				matPrim.color = vec4(0.85f, 0.30f, 0.95f, 0.85f);
				matSec.color = vec4(0.60f, 0.18f, 0.85f, 0.65f);
				matAcc.color = vec4(0.40f, 0.10f, 0.58f, 0.45f);
				matBrt.color = vec4(0.92f, 0.75f, 1.0f, 0.95f);
				matPart.color = vec4(0.88f, 0.70f, 1.0f, 0.40f);
				matCyan.color = vec4(0.88f, 0.45f, 1.0f, 0.95f);
				state.toastMessage = "THEME: AMETHYST GRID";
				break;

			case 5: // Amber Sunset Grid
				bg.primaryColor = vec4(0.07f, 0.03f, 0.01f, 1.0f);
				bg.secondaryColor = vec4(0.25f, 0.11f, 0.04f, 1.0f);
				matPrim.color = vec4(1.0f, 0.65f, 0.15f, 0.85f);
				matSec.color = vec4(0.90f, 0.45f, 0.10f, 0.65f);
				matAcc.color = vec4(0.55f, 0.22f, 0.08f, 0.45f);
				matBrt.color = vec4(1.0f, 0.90f, 0.60f, 0.95f);
				matPart.color = vec4(1.0f, 0.85f, 0.50f, 0.40f);
				matCyan.color = vec4(1.0f, 0.75f, 0.25f, 0.95f);
				state.toastMessage = "THEME: AMBER GRID";
				break;
		}

		state.toastTimer = 2.4f;
	}

	void playNavigationSound(ServiceProvider& services, float freq, float vol)
	{
		WeirdAudio::SimpleAudioRequest req;
		req.volume = vol;
		req.frequency = freq;
		req.spatial = false;
		req.beats = 1;
		req.instrument = 0; // Pure sine wave
		services.audio().playSound(req);
	}

	void playConfirmSound(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		playNavigationSound(services, 1250.0f, 0.45f);
		state.secondChimeTimer = 0.06f;
		state.secondChimeFreq = 1660.0f;
	}

	void playCancelSound(ServiceProvider& services)
	{
		playNavigationSound(services, 650.0f, 0.35f);
	}

	void executeMenuItem(Registry& registry, ServiceProvider& services, const MenuItem& item)
	{
		State& state = getState(registry);

		if (item.actionId == "launch_scene")
		{
			playConfirmSound(registry, services);
			services.sceneControl().goToNextScene(item.targetScene);
		}
		else if (item.actionId == "play_song")
		{
			if (item.songIndex >= 0 && item.songIndex < static_cast<int>(state.songs.size()))
			{
				services.audio().setSong(state.songs[item.songIndex], true);
				services.audio().music().setVolume(0.85f);
				services.audio().music().setTrackToggles({true, true, true, true});
				state.currentPlayingSong = item.songIndex;
				state.toastMessage = "PLAYING: " + item.title;
				state.toastTimer = 2.5f;
				playConfirmSound(registry, services);
			}
		}
		else if (item.actionId == "stop_song")
		{
			services.audio().setSong(nullptr, false);
			state.currentPlayingSong = -1;
			state.toastMessage = "MUSIC STOPPED";
			state.toastTimer = 2.0f;
			playCancelSound(services);
		}
		else if (item.actionId == "cycle_theme")
		{
			applyTheme(registry, services, state.currentTheme + 1);
			playConfirmSound(registry, services);
		}
		else if (item.actionId == "system_info")
		{
			state.toastMessage = "WEIRD ENGINE OS 3.80 - 112 GB FREE";
			state.toastTimer = 2.5f;
			playConfirmSound(registry, services);
		}
	}

	void menuInputSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		auto& input = services.input();

		bool leftDown = input.getKeyDown(Input::Left) || input.getKeyDown(Input::A) ||
						input.getGamepadButtonDown(Input::GamepadButton::DpadLeft);
		bool rightDown = input.getKeyDown(Input::Right) || input.getKeyDown(Input::D) ||
						 input.getGamepadButtonDown(Input::GamepadButton::DpadRight);
		bool upDown = input.getKeyDown(Input::Up) || input.getKeyDown(Input::W) ||
					  input.getGamepadButtonDown(Input::GamepadButton::DpadUp);
		bool downDown = input.getKeyDown(Input::Down) || input.getKeyDown(Input::S) ||
						input.getGamepadButtonDown(Input::GamepadButton::DpadDown);

		bool leftHeld = input.getKey(Input::Left) || input.getKey(Input::A) ||
						input.getGamepadButton(Input::GamepadButton::DpadLeft);
		bool rightHeld = input.getKey(Input::Right) || input.getKey(Input::D) ||
						 input.getGamepadButton(Input::GamepadButton::DpadRight);
		bool upHeld =
			input.getKey(Input::Up) || input.getKey(Input::W) || input.getGamepadButton(Input::GamepadButton::DpadUp);
		bool downHeld = input.getKey(Input::Down) || input.getKey(Input::S) ||
						input.getGamepadButton(Input::GamepadButton::DpadDown);

		bool confirm = input.getKeyDown(Input::Enter) || input.getKeyDown(Input::Space) ||
					   input.getGamepadButtonDown(Input::GamepadButton::South);
		bool cancel = input.getKeyDown(Input::Esc) || input.getKeyDown(Input::Backspace) ||
					  input.getGamepadButtonDown(Input::GamepadButton::East);

		int curCatMax = static_cast<int>(state.categories.size()) - 1;
		int curItemsMax = static_cast<int>(state.categories[state.selectedCategory].items.size()) - 1;

		auto moveCategory = [&](int dir)
		{
			int nextCat = std::clamp(state.selectedCategory + dir, 0, curCatMax);
			if (nextCat != state.selectedCategory)
			{
				state.categories[state.selectedCategory].selectedItem = state.selectedItem;
				state.selectedCategory = nextCat;
				int newItemsMax = static_cast<int>(state.categories[state.selectedCategory].items.size()) - 1;
				state.selectedItem = std::clamp(state.categories[state.selectedCategory].selectedItem, 0, newItemsMax);
				state.smoothItem = static_cast<float>(state.selectedItem);
				state.categoryTitleTimer = (state.selectedItem != 0) ? 1.0f : 0.0f;
				playNavigationSound(services, 1340.0f, 0.38f);
			}
			else
			{
				playNavigationSound(services, 420.0f, 0.20f);
			}
		};

		auto moveItem = [&](int dir)
		{
			int nextItem = std::clamp(state.selectedItem + dir, 0, curItemsMax);
			if (nextItem != state.selectedItem)
			{
				state.selectedItem = nextItem;
				state.categories[state.selectedCategory].selectedItem = nextItem;
				state.categoryTitleTimer = 0.0f;
				playNavigationSound(services, 1120.0f, 0.32f);
			}
			else
			{
				playNavigationSound(services, 420.0f, 0.20f);
			}
		};

		// Instant press triggers
		if (leftDown)
		{
			moveCategory(-1);
			state.activeHoldKey = -1;
			state.keyHoldTimer = 0.0f;
		}
		else if (rightDown)
		{
			moveCategory(+1);
			state.activeHoldKey = +1;
			state.keyHoldTimer = 0.0f;
		}
		else if (upDown)
		{
			moveItem(-1);
			state.activeHoldKey = -2;
			state.keyHoldTimer = 0.0f;
		}
		else if (downDown)
		{
			moveItem(+1);
			state.activeHoldKey = +2;
			state.keyHoldTimer = 0.0f;
		}

		// Mouse Navigation Support (Click on categories or items)
		if (input.getMouseButtonDown(Input::LeftClick))
		{
			float mx = input.getMouseX();
			float my = input.getMouseY();

			float screenW = static_cast<float>(Display::width);
			float screenH = static_cast<float>(Display::height);
			float focusX = screenW * 0.28f;
			float categoryY = screenH * 0.62f;
			float categorySpacing = 92.0f;
			float itemSpacingY = 56.0f;
			float iconGapY = 68.0f;
			float focusItemY = categoryY - iconGapY;

			// Check category row click
			if (std::abs(my - categoryY) < 28.0f)
			{
				int clickedCat = static_cast<int>(std::round((mx - focusX) / categorySpacing + state.smoothCategory));
				if (clickedCat >= 0 && clickedCat <= curCatMax && clickedCat != state.selectedCategory)
				{
					state.categories[state.selectedCategory].selectedItem = state.selectedItem;
					state.selectedCategory = clickedCat;
					int newItemsMax = static_cast<int>(state.categories[state.selectedCategory].items.size()) - 1;
					state.selectedItem =
						std::clamp(state.categories[state.selectedCategory].selectedItem, 0, newItemsMax);
					state.smoothItem = static_cast<float>(state.selectedItem);
					state.categoryTitleTimer = (state.selectedItem != 0) ? 1.0f : 0.0f;
					playNavigationSound(services, 1340.0f, 0.38f);
				}
			}
			// Check vertical item list click (both under and over the menu icon)
			else if (mx >= focusX - 25.0f && mx <= focusX + 320.0f)
			{
				int clickedItem = -1;
				if (my <= categoryY - 35.0f && my >= 150.0f)
				{
					// Clicked below the category icon
					float offset = (focusItemY - my) / itemSpacingY;
					clickedItem = static_cast<int>(std::round(state.smoothItem + offset));
				}
				else if (my >= categoryY + 35.0f && my <= screenH - 50.0f)
				{
					// Clicked above the category icon
					float offset = -1.0f - (my - (categoryY + iconGapY)) / itemSpacingY;
					clickedItem = static_cast<int>(std::round(state.smoothItem + offset));
				}

				if (clickedItem >= 0 && clickedItem <= curItemsMax)
				{
					if (clickedItem == state.selectedItem)
					{
						confirm = true;
					}
					else
					{
						state.selectedItem = clickedItem;
						state.categories[state.selectedCategory].selectedItem = clickedItem;
						state.categoryTitleTimer = 0.0f;
						playNavigationSound(services, 1120.0f, 0.32f);
					}
				}
			}
		}

		// Key Hold / Fast Repeat handling
		int currentlyHeldKey = 0;
		if (leftHeld)
			currentlyHeldKey = -1;
		else if (rightHeld)
			currentlyHeldKey = +1;
		else if (upHeld)
			currentlyHeldKey = -2;
		else if (downHeld)
			currentlyHeldKey = +2;

		if (currentlyHeldKey != 0 && currentlyHeldKey == state.activeHoldKey)
		{
			state.keyHoldTimer += delta;
			constexpr float INITIAL_DELAY = 0.28f;
			constexpr float REPEAT_INTERVAL = 0.09f;

			if (state.keyHoldTimer > INITIAL_DELAY)
			{
				state.keyRepeatTimer += delta;
				if (state.keyRepeatTimer >= REPEAT_INTERVAL)
				{
					state.keyRepeatTimer = 0.0f;
					if (state.activeHoldKey == -1)
						moveCategory(-1);
					else if (state.activeHoldKey == +1)
						moveCategory(+1);
					else if (state.activeHoldKey == -2)
						moveItem(-1);
					else if (state.activeHoldKey == +2)
						moveItem(+1);
				}
			}
		}
		else
		{
			state.activeHoldKey = currentlyHeldKey;
			state.keyHoldTimer = 0.0f;
			state.keyRepeatTimer = 0.0f;
		}

		// Action trigger
		if (confirm)
		{
			const auto& curItem = state.categories[state.selectedCategory].items[state.selectedItem];
			executeMenuItem(registry, services, curItem);
		}

		// Cancel / Back (reset to top of current category)
		if (cancel)
		{
			if (state.selectedItem > 0)
			{
				state.selectedItem = 0;
				state.categories[state.selectedCategory].selectedItem = 0;
				playCancelSound(services);
			}
			state.toastTimer = 0.0f;
			state.categoryTitleTimer = 0.0f;
		}

		// Delayed second chime for confirmation sound
		if (state.secondChimeTimer > 0.0f)
		{
			state.secondChimeTimer -= delta;
			if (state.secondChimeTimer <= 0.0f)
			{
				playNavigationSound(services, state.secondChimeFreq, 0.40f);
			}
		}

		// Continuously remember last menu position during this session
		s_menuPositionSaved = true;
		s_savedCategory = state.selectedCategory;
		s_savedItem = state.selectedItem;
		if (state.selectedCategory >= 0 && state.selectedCategory < static_cast<int>(State::CATEGORY_COUNT))
		{
			s_savedCategoryItems[state.selectedCategory] = state.selectedItem;
		}
		s_savedTheme = state.currentTheme;
	}

	void menuAnimationSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();
		float time = services.time().time();

		// Exponential critically damped smoothing (PS3 snappy feel)
		constexpr float LERP_FACTOR = 18.0f;
		float alpha = 1.0f - std::exp(-LERP_FACTOR * delta);

		state.smoothCategory += (static_cast<float>(state.selectedCategory) - state.smoothCategory) * alpha;
		state.smoothItem += (static_cast<float>(state.selectedItem) - state.smoothItem) * alpha;

		// Category title timer countdown
		if (state.categoryTitleTimer > 0.0f)
		{
			state.categoryTitleTimer = std::max(0.0f, state.categoryTitleTimer - delta);
		}

		float screenW = static_cast<float>(Display::width);
		float screenH = static_cast<float>(Display::height);

		// Layout constants
		float focusX = screenW * 0.28f;
		float categoryY = screenH * 0.62f;
		float categorySpacing = 92.0f;

		float itemSpacingY = 56.0f;
		float iconGapY = 68.0f;
		float focusItemY = categoryY - iconGapY; // Selected item padded under the icon

		// 1. Update Horizontal Categories UI Shapes (All 3 categories have 100% identical outer size)
		for (size_t i = 0; i < State::CATEGORY_COUNT; ++i)
		{
			float catOffset = static_cast<float>(i) - state.smoothCategory;
			float posX = focusX + catOffset * categorySpacing;
			float posY = categoryY;

			float distToFocus = std::abs(catOffset);
			float focusWeight = std::clamp(1.0f - distToFocus, 0.0f, 1.0f);

			// Outer icon: uniform CircleLine, EXACT SAME outer size for all 3 categories
			float outerRadius = 15.0f + 4.0f * focusWeight; // 15px unfocused, 19px focused

			if (registry.isEntityValid(state.categoryOuterIcons[i]))
			{
				auto& ui = registry.getComponent<UIShape>(state.categoryOuterIcons[i]);
				ui.parameters[0] = posX;
				ui.parameters[1] = posY;
				ui.parameters[2] = outerRadius;
				ui.parameters[3] = 1.6f; // Uniform stroke thickness for all
				registry.setComponentDirty(ui);
			}

			// Inner icon: distinct, well-proportioned glyphs inside each category
			if (registry.isEntityValid(state.categoryInnerIcons[i]))
			{
				auto& ui = registry.getComponent<UIShape>(state.categoryInnerIcons[i]);
				ui.parameters[0] = posX;
				ui.parameters[1] = posY;

				if (i == 0) // GAME: Console / gamepad rectangle
				{
					ui.parameters[2] = 6.0f + 1.5f * focusWeight; // half-width
					ui.parameters[3] = 3.6f + 1.0f * focusWeight; // half-height
				}
				else if (i == 1) // MUSIC: Optical disc hub / spindle circle
				{
					ui.parameters[2] = 4.5f + 1.2f * focusWeight; // radius
				}
				else if (i == 2) // SETTINGS: Diamond dial (BoxRotated 45 deg)
				{
					float diamondHalf = 4.2f + 1.0f * focusWeight;
					ui.parameters[2] = diamondHalf; // half-width
					ui.parameters[3] = diamondHalf; // half-height
					ui.parameters[4] = 0.785398f;	// 45 degrees
				}

				registry.setComponentDirty(ui);
			}
		}

		// 2. Active Category Focus Halo Ring
		if (registry.isEntityValid(state.categoryFocusRing))
		{
			float ringPulse = 25.0f + 1.2f * std::sin(time * 3.5f);
			auto& ui = registry.getComponent<UIShape>(state.categoryFocusRing);
			ui.parameters[0] = focusX;
			ui.parameters[1] = categoryY;
			ui.parameters[2] = ringPulse;
			ui.parameters[3] = 1.8f;
			registry.setComponentDirty(ui);
		}

		// 3. Active Category Title Text (Shown when at root/top of list, or temporarily on category switch)
		if (registry.isEntityValid(state.activeCategoryLabel))
		{
			auto& t = registry.getComponent<Transform>(state.activeCategoryLabel);
			t.position.x = focusX;
			t.position.y = categoryY + 48.0f;
			registry.setComponentDirty(t);

			auto& tr = registry.getComponent<UITextRenderer>(state.activeCategoryLabel);
			bool showTitle = (state.selectedItem == 0) || (state.categoryTitleTimer > 0.0f);
			tr.text = showTitle ? state.categories[state.selectedCategory].name : "";
			registry.setComponentDirty(tr);
		}

		// 4. Update Vertical Sub-Items Pool (9 slots: -4 to +4)
		const auto& items = state.categories[state.selectedCategory].items;
		int itemCount = static_cast<int>(items.size());

		const float maxVisibleY = screenH - 50.0f; // Screen top bounds
		const float minVisibleY = 150.0f;		   // Screen bottom bounds

		for (int slot = 0; slot < State::VISIBLE_ITEM_SLOTS; ++slot)
		{
			int relIndex = slot - 4; // Center slot is index 4
			int itemIndex = state.selectedItem + relIndex;

			bool slotValid = (itemIndex >= 0 && itemIndex < itemCount);
			float posY = -1000.0f;
			float posX = focusX;
			bool isVisible = false;

			if (slotValid)
			{
				float offset = static_cast<float>(itemIndex) - state.smoothItem;
				if (offset >= 0.0f)
				{
					// Selected item (offset=0, at focusItemY padded under the icon) and items below it
					posY = focusItemY - offset * itemSpacingY;
				}
				else if (offset >= -1.0f)
				{
					// Moving upwards across the menu icon with iconGapY padding
					posY = focusItemY - offset * (2.0f * iconGapY);
				}
				else
				{
					// Items sitting over (above) the menu icon, spaced by itemSpacingY
					posY = (categoryY + iconGapY) - (offset + 1.0f) * itemSpacingY;
				}

				isVisible = (posY <= maxVisibleY && posY >= minVisibleY);

				// When category title is temporarily shown, hide items sitting over the category icon
				if (state.categoryTitleTimer > 0.0f && itemIndex < state.selectedItem)
				{
					isVisible = false;
				}
			}

			if (slotValid && isVisible)
			{
				// Update Label: cleanly positioned, left-aligned with a slight indent from the icon
				if (registry.isEntityValid(state.itemLabels[slot]))
				{
					auto& t = registry.getComponent<Transform>(state.itemLabels[slot]);
					t.position.x = posX + 6.0f;
					t.position.y = posY;
					registry.setComponentDirty(t);

					auto& tr = registry.getComponent<UITextRenderer>(state.itemLabels[slot]);
					tr.text = items[itemIndex].title;
					tr.material = (relIndex == 0) ? services.materials2D().get("ui_white").id
												  : services.materials2D().get("ui_dimmed").id;
					registry.setComponentDirty(tr);
				}
			}
			else
			{
				// Hide offscreen, invalid, or clipped slots
				if (registry.isEntityValid(state.itemLabels[slot]))
				{
					auto& t = registry.getComponent<Transform>(state.itemLabels[slot]);
					t.position.x = -1000.0f;
					t.position.y = -1000.0f;
					registry.setComponentDirty(t);

					auto& tr = registry.getComponent<UITextRenderer>(state.itemLabels[slot]);
					tr.text = "";
					registry.setComponentDirty(tr);
				}
			}
		}

		// 6. Toast Notification Banner
		if (state.toastTimer > 0.0f)
		{
			state.toastTimer -= delta;
			if (registry.isEntityValid(state.toastLabel))
			{
				auto& t = registry.getComponent<Transform>(state.toastLabel);
				t.position.x = focusX - 20.0f;
				t.position.y = 120.0f;
				registry.setComponentDirty(t);

				auto& tr = registry.getComponent<UITextRenderer>(state.toastLabel);
				tr.text = state.toastMessage;
				registry.setComponentDirty(tr);
			}
		}
		else
		{
			if (registry.isEntityValid(state.toastLabel))
			{
				auto& tr = registry.getComponent<UITextRenderer>(state.toastLabel);
				tr.text = "";
				registry.setComponentDirty(tr);
			}
		}
	}

	void worldWaveAnimationSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float time = services.time().time();

		// Update drifting ambient particles in the 2D raymarching world
		for (auto& p : state.particles)
		{
			if (registry.isEntityValid(p.entity))
			{
				float curX = std::fmod(p.baseX + p.speedX * time + 72.0f, 72.0f) - 36.0f;
				float curY = p.baseY + std::sin(time * p.freqY + p.phase) * p.ampY;
				float curRadius = p.baseRadius * (0.85f + 0.15f * std::sin(time * 2.5f + p.phase));

				auto& cs = registry.getComponent<Shape>(p.entity);
				cs.parameters[DefaultShapes::Circle::PosX] = curX;
				cs.parameters[DefaultShapes::Circle::PosY] = curY;
				cs.parameters[DefaultShapes::Circle::Radius] = curRadius;
				registry.setComponentDirty(cs);
			}
		}

		// Pure horizontal camera parallax responding only to horizontal menu navigation (strictly no vertical motion)
		float catOffset = state.smoothCategory - 1.0f; // -1: GAME, 0: MUSIC, +1: SETTINGS
		float parallaxX = catOffset * 3.5f;

		// Camera is completely rock-solid and static at rest (zero idle drift/wobble)
		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position.x = parallaxX;
		cameraTransform.position.y = 0.0f;
		cameraTransform.position.z = 25.0f;
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
		registry.setComponentDirty(cameraTransform);
	}

	void statusUpdateSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Minimal Clock
		if (registry.isEntityValid(state.clockLabel))
		{
			std::time_t now = std::time(nullptr);
			std::tm localTm{};
#if defined(_WIN32)
			localtime_s(&localTm, &now);
#else
			localtime_r(&now, &localTm);
#endif
			char clockBuf[16];
			std::strftime(clockBuf, sizeof(clockBuf), "%H:%M", &localTm);

			auto& tr = registry.getComponent<UITextRenderer>(state.clockLabel);
			if (tr.text != clockBuf)
			{
				tr.text = clockBuf;
				registry.setComponentDirty(tr);
			}
		}
	}

	void responsiveLayoutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		int hash = Display::width + Display::height;
		if (hash != state.lastResolutionHash)
		{
			state.lastResolutionHash = hash;

			float screenW = static_cast<float>(Display::width);
			float screenH = static_cast<float>(Display::height);

			if (registry.isEntityValid(state.clockLabel))
			{
				auto& t = registry.getComponent<Transform>(state.clockLabel);
				t.position.x = screenW - 40.0f;
				t.position.y = screenH - 40.0f;
				registry.setComponentDirty(t);
			}
		}
	}

} // namespace MenuSceneNamespace
