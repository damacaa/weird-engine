#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <weird-engine.h>

namespace ImageSceneNamespace
{
	struct MemoryImage
	{
		int width = 0;
		int height = 0;
		std::vector<uint8_t> rgb;
	};

	struct State
	{
		std::string cacheFilePath = "cache/image.txt";
		std::string currentImagePath;

		MemoryImage loadedImage;
		std::vector<int> savedMaterials;
		std::vector<WeirdEngine::Entity> balls;

		// Chamber shapes
		WeirdEngine::Entity leftWallEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity rightWallEntity = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity floorEntity = WeirdEngine::INVALID_ENTITY;

		// UI Buttons (bottom-left aligned)
		WeirdEngine::Entity loadButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity saveButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity rerunButton = WeirdEngine::INVALID_ENTITY;

		// Thread-safe file dialog state
		std::mutex fileMutex;
		std::string pendingLoadPath;

		int lastResolutionHash = 0;
		int imageWidth = 32;
		int imageHeight = 32;
		bool hasSavedMaterials = false;
	};

	State& getState(WeirdEngine::Registry& registry);
	void openImageFileDialog(State& state);
	void saveMaterials(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, State& state);
	void loadImage(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, State& state,
				   const std::string& path);
	void spawnBalls(WeirdEngine::Registry& registry, State& state, bool useSavedMaterials);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupChamberSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupControlsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void loadInitialImageSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void fileDialogSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void buttonSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void shortcutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void layoutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace ImageSceneNamespace

class ImageScene : public WeirdEngine::Scene2D
{
public:
	ImageScene()
	{
		addStartSystem(ImageSceneNamespace::stateInitSystem);
		addStartSystem(ImageSceneNamespace::setupChamberSystem);
		addStartSystem(ImageSceneNamespace::setupControlsSystem);
		addStartSystem(ImageSceneNamespace::loadInitialImageSystem);
		addStartSystem(ImageSceneNamespace::cameraInitSystem);

		addUpdateSystem(ImageSceneNamespace::sceneControlSystem);
		addUpdateSystem(ImageSceneNamespace::fileDialogSystem);
		addUpdateSystem(ImageSceneNamespace::buttonSystem);
		addUpdateSystem(ImageSceneNamespace::shortcutSystem);
		addUpdateSystem(ImageSceneNamespace::layoutSystem);
		addUpdateSystem(ImageSceneNamespace::cameraTrackingSystem);
	}
};
