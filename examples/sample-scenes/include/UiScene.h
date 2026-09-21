#pragma once

#include <weird-engine.h>

namespace UiSceneNamespace
{
	struct State
	{
		int clickCount = 0;
		int lastResolutionHash = 0;
		bool darkMode = false;

		// UI Button / Toggle Entities
		WeirdEngine::Entity clickButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity resetButton = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity modeToggle = WeirdEngine::INVALID_ENTITY;

		// UI Text Entities (Screen space)
		WeirdEngine::Entity titleText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity counterText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity centerText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity leftText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity rightText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity mouseScreenText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity toggleLabelText = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity hintText = WeirdEngine::INVALID_ENTITY;
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupUiMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupButtonsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupTextLabelsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void buttonInteractionSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void labelUpdateSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void mouseTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void responsiveLayoutSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace UiSceneNamespace

class UiScene : public WeirdEngine::Scene2D
{
public:
	UiScene()
	{
		addStartSystem(UiSceneNamespace::stateInitSystem);
		addStartSystem(UiSceneNamespace::setupEnvironmentSystem);
		addStartSystem(UiSceneNamespace::setupUiMaterialsSystem);
		addStartSystem(UiSceneNamespace::setupButtonsSystem);
		addStartSystem(UiSceneNamespace::setupTextLabelsSystem);
		addStartSystem(UiSceneNamespace::cameraInitSystem);

		addUpdateSystem(UiSceneNamespace::sceneControlSystem);
		addUpdateSystem(UiSceneNamespace::buttonInteractionSystem);
		addUpdateSystem(UiSceneNamespace::labelUpdateSystem);
		addUpdateSystem(UiSceneNamespace::mouseTrackingSystem);
		addUpdateSystem(UiSceneNamespace::responsiveLayoutSystem);
		addUpdateSystem(UiSceneNamespace::cameraTrackingSystem);
	}
};
