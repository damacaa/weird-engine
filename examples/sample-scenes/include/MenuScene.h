#pragma once

#include "GlobalSystems.h"
#include <memory>
#include <string>
#include <vector>
#include <weird-audio/SdfSong.h>
#include <weird-engine.h>

namespace MenuSceneNamespace
{
	struct MenuItem
	{
		std::string title;
		std::string subtitle;
		std::string actionId;
		std::string targetScene;
		int songIndex = -1;
	};

	struct MenuCategory
	{
		std::string name;
		std::vector<MenuItem> items;
		int selectedItem = 0;
	};

	struct Particle
	{
		float baseX = 0.0f;
		float baseY = 0.0f;
		float speedX = 0.0f;
		float freqY = 0.0f;
		float ampY = 0.0f;
		float phase = 0.0f;
		float baseRadius = 0.5f;
		WeirdEngine::Entity entity = WeirdEngine::INVALID_ENTITY;
	};

	struct State
	{
		// Navigation State: 0 = GAME, 1 = MUSIC, 2 = SETTINGS
		int selectedCategory = 0;
		int selectedItem = 0;
		float smoothCategory = 0.0f;
		float smoothItem = 0.0f;

		// Key repeat timers
		float keyHoldTimer = 0.0f;
		float keyRepeatTimer = 0.0f;
		int activeHoldKey = 0; // -1: Left, +1: Right, -2: Up, +2: Down

		// Resolution cache
		int lastResolutionHash = 0;

		// Visual Theme
		int currentTheme = 0; // 0: Midnight Grid, 1: Obsidian, 2: Crimson, 3: Emerald, 4: Amethyst, 5: Amber

		// Interactive Notification Toast
		std::string toastMessage = "";
		float toastTimer = 0.0f;

		// Category title timer when navigating horizontally to a non-zero item
		float categoryTitleTimer = 0.0f;

		// Audio chime timer
		float secondChimeTimer = 0.0f;
		float secondChimeFreq = 0.0f;

		// Procedural Music Songs (SdfSong instances)
		std::vector<std::shared_ptr<WeirdEngine::WeirdAudio::SdfSong>> songs;
		int currentPlayingSong = -1;

		// Background World Shapes (Dynamic Wave ribbons & particles)
		WeirdEngine::ShapeId waveRibbonSdfId = 0;
		WeirdEngine::Entity waveRibbons[4] = {WeirdEngine::INVALID_ENTITY};
		std::vector<Particle> particles;

		// UI Shapes: Category Bar (GAME, MUSIC, SETTINGS)
		static constexpr size_t CATEGORY_COUNT = 3;
		WeirdEngine::Entity categoryOuterIcons[CATEGORY_COUNT] = {WeirdEngine::INVALID_ENTITY};
		WeirdEngine::Entity categoryInnerIcons[CATEGORY_COUNT] = {WeirdEngine::INVALID_ENTITY};
		WeirdEngine::Entity categoryFocusRing = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity activeCategoryLabel = WeirdEngine::INVALID_ENTITY;

		// UI Entities: Vertical Sub-Items Pool (slots relative to focal row)
		static constexpr int VISIBLE_ITEM_SLOTS = 9; // -4 to +4 relative to active
		WeirdEngine::Entity itemLabels[VISIBLE_ITEM_SLOTS] = {WeirdEngine::INVALID_ENTITY};

		// Info / Toast display
		WeirdEngine::Entity toastLabel = WeirdEngine::INVALID_ENTITY;

		// Top Clock
		WeirdEngine::Entity clockLabel = WeirdEngine::INVALID_ENTITY;

		// Menu Content
		std::vector<MenuCategory> categories;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupWorldWaveSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupCategoryUiSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupItemUiSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupStatusUiSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);

	void menuInputSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void menuAnimationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void worldWaveAnimationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void statusUpdateSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void responsiveLayoutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);

	void applyTheme(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, int themeIndex);
	void playNavigationSound(WeirdEngine::ServiceProvider& services, float freq, float vol = 0.35f);
	void playConfirmSound(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void playCancelSound(WeirdEngine::ServiceProvider& services);
} // namespace MenuSceneNamespace

class MenuScene : public WeirdEngine::Scene2D
{
public:
	MenuScene()
	{
		addStartSystem(MenuSceneNamespace::stateInitSystem);
		addStartSystem(MenuSceneNamespace::setupEnvironmentSystem);
		addStartSystem(MenuSceneNamespace::setupMaterialsSystem);
		addStartSystem(MenuSceneNamespace::setupWorldWaveSystem);
		addStartSystem(MenuSceneNamespace::setupCategoryUiSystem);
		addStartSystem(MenuSceneNamespace::setupItemUiSystem);
		addStartSystem(MenuSceneNamespace::setupStatusUiSystem);

		addUpdateSystem(MenuSceneNamespace::menuInputSystem);
		addUpdateSystem(MenuSceneNamespace::menuAnimationSystem);
		addUpdateSystem(MenuSceneNamespace::worldWaveAnimationSystem);
		addUpdateSystem(MenuSceneNamespace::statusUpdateSystem);
		addUpdateSystem(MenuSceneNamespace::responsiveLayoutSystem);
	}
};
