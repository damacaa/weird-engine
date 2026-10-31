#include "MenuScene.h"
#include "MusicShapes.h"

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

	glm::vec2 getClockPreviewCenter(Registry& registry)
	{
		State& state = getState(registry);
		float screenW = static_cast<float>(Display::width > 0 ? Display::width : 800);
		float screenH = static_cast<float>(Display::height > 0 ? Display::height : 800);

		float clockWidth = 190.0f;
		float clockHeight = 40.0f;
		float clockRight = screenW - 40.0f;
		float clockTop = screenH - 40.0f;

		if (registry.isEntityValid(state.clockLabel) && registry.hasComponent<UITextRenderer>(state.clockLabel))
		{
			const auto& tr = registry.getComponent<UITextRenderer>(state.clockLabel);
			const auto& t = registry.getComponent<Transform>(state.clockLabel);
			clockRight = t.position.x;
			clockTop = t.position.y;
			if (tr.width > 0.0f)
			{
				clockWidth = tr.width;
			}
			if (tr.height > 0.0f)
			{
				clockHeight = tr.height;
			}
		}

		float centerX = clockRight - (clockWidth * 0.5f);
		float centerY = (clockTop - clockHeight) - 15.0f - SdfSong::DOMAIN_RADIUS;
		return glm::vec2(centerX, centerY);
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		auto& state = registry.emplaceState<State>();

		// Initialize 3 Categories: SCENES, MUSIC, SETTINGS
		state.categories = {
			MenuCategory{
				"SCENES",
				{
					{"SHAPES", "2D Signed Distance Field shapes showcase", "launch_scene", "shapes"},
					{"ROPE", "Verlet physics rope simulation", "launch_scene", "rope"},
					{"PLATFORMER", "2D character platformer with terrain collision", "launch_scene", "platformer"},
					{"LIFE", "Game of Life cellular automata", "launch_scene", "life"},
					{"COLLISIONS", "Interactive particle collision dynamics", "launch_scene", "collisions"},
					{"IMAGE", "Image Signed Distance Field processing", "launch_scene", "image"},
					{"MUSIC", "Original Music Scene visualizer", "launch_scene", "music"},
				}},
			MenuCategory{"MUSIC",
						 {
							 {"TRACK 01", "Midnight Pulse (Synthwave Beat)", "play_song", "", 0},
							 {"TRACK 02", "Silica Dreams (Ambient Chill)", "play_song", "", 1},
							 {"TRACK 03", "Velocity (Electronic Melody)", "play_song", "", 2},
							 {"TRACK 04", "Vortex (Hypnotic Rhythm)", "play_song", "", 3},
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
			state.selectedCategory = 0; // Default to SCENES
			state.selectedItem = 0;
			state.smoothCategory = 0.0f;
			state.smoothItem = 0.0f;
			state.currentTheme = 0;
		}

		glm::vec2 previewCenter = getClockPreviewCenter(registry);

		// Instantiate procedural songs using shared MusicShapes definitions
		state.songs = MusicShapes::createAllSongs(previewCenter);
		state.currentPlayingSong = -1;
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(true);
		services.debug().setDebugFly(false);

		// Camera looking at XY plane from Z=25, aligned with restored category parallax
		float catOffset = state.smoothCategory - 1.0f; // -1: SCENES, 0: MUSIC, +1: SETTINGS
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

		// Apply active theme (restored or default Classic)
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

	// Custom Procedural Category Icon Shapes (SCENES, MUSIC, SETTINGS)
	namespace
	{
		inline Expr getScenesIconShape(const Vec2Expr& p, const Expr& s, const Expr& angle)
		{
			Vec2Expr rotP = rotate(p, angle);
			// 16-point star shape
			return sdStar(rotP, s * 0.70f, s * 0.10f, 16.0f, 0.0f);
		}

		inline Expr getMusicIconShape(const Vec2Expr& p, const Expr& s)
		{
			// Center vertically: note heads at bottom, beam at top
			Vec2Expr pC = p - Vec2Expr(Expr(0.0f), s * 0.06f);

			// Left and right note heads: angled ovals
			Expr headL = sdfRound(
				sdBox(rotate(pC - Vec2Expr(-s * 0.32f, -s * 0.30f), Expr(0.35f)), Vec2Expr(s * 0.18f, s * 0.11f)),
				s * 0.08f);
			Expr headR = sdfRound(
				sdBox(rotate(pC - Vec2Expr(s * 0.26f, -s * 0.12f), Expr(0.35f)), Vec2Expr(s * 0.18f, s * 0.11f)),
				s * 0.08f);

			// Vertical stems connecting heads to top beam
			Expr stemL = sdLine(pC, Vec2Expr(-s * 0.16f, -s * 0.28f), Vec2Expr(-s * 0.16f, s * 0.44f), s * 0.055f);
			Expr stemR = sdLine(pC, Vec2Expr(s * 0.42f, -s * 0.10f), Vec2Expr(s * 0.42f, s * 0.60f), s * 0.055f);

			// Bold angled crossbeam
			Expr beam = sdLine(pC, Vec2Expr(-s * 0.20f, s * 0.38f), Vec2Expr(s * 0.46f, s * 0.54f), s * 0.11f);

			Expr heads = sdfUnion(headL, headR);
			Expr stems = sdfUnion(stemL, stemR);
			return sdfUnion(heads, sdfUnion(stems, beam));
		}

		inline Expr getSettingsIconShape(const Vec2Expr& p, const Expr& s, const Expr& angle)
		{
			Vec2Expr rotP = rotate(p, angle);

			// Central circular disc
			Expr wheel = sdCircle(rotP, s * 0.70f);

			// 6 radial teeth formed by 3 crossed rounded bars (0 deg, 60 deg, 120 deg)
			Expr bar0 = sdfRound(sdBox(rotP, Vec2Expr(s * 0.86f, s * 0.16f)), s * 0.06f);
			Expr bar1 = sdfRound(sdBox(rotate(rotP, Expr(1.04719755f)), Vec2Expr(s * 0.86f, s * 0.16f)), s * 0.06f);
			Expr bar2 = sdfRound(sdBox(rotate(rotP, Expr(2.09439510f)), Vec2Expr(s * 0.86f, s * 0.16f)), s * 0.06f);

			Expr teeth = sdfUnion(bar0, sdfUnion(bar1, bar2));
			Expr cog = sdfUnion(wheel, teeth);

			// Center axle hole
			Expr axleHole = sdCircle(rotP, s * 0.28f);

			return sdfSubtract(cog, axleHole);
		}
	} // namespace

	void setupCategoryUiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& uiDimmed = services.materials2D().get("ui_dimmed");
		auto& uiCyan = services.materials2D().get("ui_cyan");
		auto& uiWhite = services.materials2D().get("ui_white");

		// Register custom procedural SDF icons: SCENES (16-point star), MUSIC (beamed notes), SETTINGS (gear)
		Vec2Expr pScenes = translate(point(), Vec2Expr(var(0), var(1)));
		state.categoryShapeIds[0] = services.shapes().registerSDF(getScenesIconShape(pScenes, var(2), var(3)));

		Vec2Expr pMusic = translate(point(), Vec2Expr(var(0), var(1)));
		state.categoryShapeIds[1] = services.shapes().registerSDF(getMusicIconShape(pMusic, var(2)));

		Vec2Expr pSettings = translate(point(), Vec2Expr(var(0), var(1)));
		state.categoryShapeIds[2] = services.shapes().registerSDF(getSettingsIconShape(pSettings, var(2), var(3)));

		for (size_t i = 0; i < State::CATEGORY_COUNT; ++i)
		{
			state.categoryIcons[i] = services.shapes().addUIShape({
				.shapeId = state.categoryShapeIds[i],
				.variables = {0.0f, 0.0f, 32.0f, 0.0f},
				.material = (static_cast<int>(i) == state.selectedCategory ? uiWhite.id : uiDimmed.id),
			});
		}
		state.lastRenderedCategory = state.selectedCategory;

		// Active Category Focus Indicator (small triangle at the left pointing at the icon)
		{
			state.categoryIndicator = services.shapes().addUIShape({
				.shapeId = DefaultShapes::TriangleRotated,
				.variables = {{DefaultShapes::TriangleRotated::PosX, 0.0f},
							  {DefaultShapes::TriangleRotated::PosY, 0.0f},
							  {DefaultShapes::TriangleRotated::Width, 14.0f},
							  {DefaultShapes::TriangleRotated::Height, 14.0f},
							  {DefaultShapes::TriangleRotated::Angle, 1.5707963f}},
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
		auto& uiDimmed = services.materials2D().get("ui_dimmed");

		// Header: Minimal Clock text in top-right
		{
			state.clockLabel = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.clockLabel);
			t.position =
				vec3(static_cast<float>(Display::width) - 40.0f, static_cast<float>(Display::height) - 40.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.clockLabel);
			tr.text = "12:00";
			tr.material = uiDimmed.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		// Reconnect song preview if background music is already playing
		auto activeSong = services.audio().music().getCurrentSong();
		if (activeSong && services.audio().music().getVolume() > 0.0f)
		{
			for (size_t i = 0; i < state.songs.size(); ++i)
			{
				if (state.songs[i]->getName() == activeSong->getName())
				{
					state.songs[i] = activeSong;
					state.currentPlayingSong = static_cast<int>(i);
					break;
				}
			}

			if (state.currentPlayingSong >= 0)
			{
				glm::vec2 previewCenter = getClockPreviewCenter(registry);
				state.songs[state.currentPlayingSong]->setCenter(previewCenter);

				auto& uiCyan = services.materials2D().get("ui_cyan");
				services.audio().setSong(
					state.songs[state.currentPlayingSong],
					SongVisualizationOptions{.mode = SongVisualizationMode::UI, .material = uiCyan.id}, false);
			}
		}
	}

	void applyTheme(Registry& registry, ServiceProvider& services, int themeIndex)
	{
		State& state = getState(registry);
		state.currentTheme = (themeIndex % 7 + 7) % 7;

		auto& bg = services.render().getBackground();
		bg.type = BackgroundType::Grid;
		bg.scale = 4.0f;
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
			case 0: // Classic
				bg.primaryColor = vec4(0.7f, 0.7f, 0.71f, 1.0f);
				bg.secondaryColor = vec4(0.55f, 0.55f, 0.58f, 1.0f);
				matPrim.color = ColorPalette::Red;
				matSec.color = ColorPalette::Green;
				matAcc.color = ColorPalette::Blue;
				matBrt.color = ColorPalette::Yellow;
				matPart.color = ColorPalette::Magenta;
				matCyan.color = ColorPalette::Cyan;
				state.toastMessage = "THEME: CLASSIC GRID";
				break;

			case 1: // Classic Midnight Grid
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

			case 2: // Obsidian Onyx Grid
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

			case 3: // Crimson Silk Grid
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

			case 4: // Emerald Aurora Grid
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

			case 5: // Amethyst Purple Grid
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

			case 6: // Amber Sunset Grid
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

	void updateCategoryIconMaterials(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		auto& uiDimmed = services.materials2D().get("ui_dimmed");
		auto& uiWhite = services.materials2D().get("ui_white");

		bool changed = false;
		for (size_t i = 0; i < State::CATEGORY_COUNT; ++i)
		{
			if (registry.isEntityValid(state.categoryIcons[i]))
			{
				auto& ui = registry.getComponent<UIShape>(state.categoryIcons[i]);
				uint16_t targetMat = (static_cast<int>(i) == state.selectedCategory ? uiWhite.id : uiDimmed.id);
				if (ui.material != targetMat)
				{
					ui.material = targetMat;
					registry.setComponentDirty(ui);
					changed = true;
				}
			}
		}

		if (changed)
		{
			services.render().forceShaderRefreshUI();
		}
		state.lastRenderedCategory = state.selectedCategory;
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
				glm::vec2 previewCenter = getClockPreviewCenter(registry);
				state.songs[item.songIndex]->setCenter(previewCenter);

				auto& uiCyan = services.materials2D().get("ui_cyan");
				services.audio().setSong(
					state.songs[item.songIndex],
					SongVisualizationOptions{.mode = SongVisualizationMode::UI, .material = uiCyan.id}, true);
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
			services.audio().music().setTrackToggles({false, false, false, false});
			services.audio().music().setVolume(0.0f);
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
				updateCategoryIconMaterials(registry, services);
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
					updateCategoryIconMaterials(registry, services);
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
		float categorySpacing = 116.0f;

		float itemSpacingY = 56.0f;
		float iconGapY = 88.0f;
		float focusItemY = categoryY - iconGapY; // Selected item padded under the icon

		// 1. Update Horizontal Categories UI Shapes (Custom procedural shapes)
		if (state.lastRenderedCategory != state.selectedCategory)
		{
			updateCategoryIconMaterials(registry, services);
		}

		// Update continuous rotation angle for SCENES 16-point star (slow when unselected, fast when selected)
		float scenesFocusWeight = std::clamp(1.0f - std::abs(state.smoothCategory), 0.0f, 1.0f);
		float starSpeed = 0.1f + 0.5f * scenesFocusWeight;
		state.scenesStarAngle += delta * starSpeed;

		for (size_t i = 0; i < State::CATEGORY_COUNT; ++i)
		{
			float catOffset = static_cast<float>(i) - state.smoothCategory;
			float posX = focusX + catOffset * categorySpacing;
			float posY = categoryY;

			float distToFocus = std::abs(catOffset);
			float focusWeight = std::clamp(1.0f - distToFocus, 0.0f, 1.0f);

			// Scale icon smoothly: 32px unfocused, 42px focused
			float iconScale = 32.0f + 10.0f * focusWeight;

			if (registry.isEntityValid(state.categoryIcons[i]))
			{
				auto& ui = registry.getComponent<UIShape>(state.categoryIcons[i]);
				ui.parameters[0] = posX;
				ui.parameters[1] = posY;
				ui.parameters[2] = iconScale;

				if (i == 0) // SCENES: 16-point star continuous rotation
				{
					ui.parameters[3] = state.scenesStarAngle;
				}
				else if (i == 2) // SETTINGS: gear rotates smoothly with horizontal navigation
				{
					ui.parameters[3] = 0.26f + state.smoothCategory * 1.5f;
				}
				else
				{
					ui.parameters[3] = 0.0f;
				}

				registry.setComponentDirty(ui);
			}
		}

		// 2. Active Category Focus Indicator (small triangle at the left of current icon, pointing at it)
		if (registry.isEntityValid(state.categoryIndicator))
		{
			auto& ui = registry.getComponent<UIShape>(state.categoryIndicator);
			ui.parameters[DefaultShapes::TriangleRotated::PosX] = focusX - 58.0f;
			ui.parameters[DefaultShapes::TriangleRotated::PosY] = categoryY;
			ui.parameters[DefaultShapes::TriangleRotated::Width] = 14.0f;
			ui.parameters[DefaultShapes::TriangleRotated::Height] = 14.0f;
			ui.parameters[DefaultShapes::TriangleRotated::Angle] =
				1.5707963f; // 90 degrees, pointing right towards icon
			registry.setComponentDirty(ui);
		}

		// 3. Active Category Title Text (Shown when at root/top of list, or temporarily on category switch)
		if (registry.isEntityValid(state.activeCategoryLabel))
		{
			auto& t = registry.getComponent<Transform>(state.activeCategoryLabel);
			t.position.x = focusX;
			t.position.y = categoryY + 62.0f;
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
		float catOffset = state.smoothCategory - 1.0f; // -1: SCENES, 0: MUSIC, +1: SETTINGS
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

			// Keep live preview centered right under the clock across window resizes
			glm::vec2 previewCenter = getClockPreviewCenter(registry);
			for (auto& song : state.songs)
			{
				if (song)
				{
					song->setCenter(previewCenter);
				}
			}

			if (state.currentPlayingSong >= 0 && state.currentPlayingSong < static_cast<int>(state.songs.size()))
			{
				auto& uiCyan = services.materials2D().get("ui_cyan");
				services.audio().setSong(
					state.songs[state.currentPlayingSong],
					SongVisualizationOptions{.mode = SongVisualizationMode::UI, .material = uiCyan.id}, false);
			}
		}
	}

} // namespace MenuSceneNamespace
