#pragma once

#include <array>
#include <cstdint>
#include <glm/glm.hpp>
#include <weird-engine.h>

#include "GlobalSystems.h"
#include "weird-engine/math/Default2DSDFs.h"

namespace WeirdEngine
{
	namespace AquariumSceneNamespace
	{
		constexpr float TANK_HALF_W = 53.3f;
		constexpr float TANK_BOTTOM_Y = -34.3f;
		constexpr float WATERLINE_Y = 28.5f;
		constexpr float TANK_TOP_Y = 34.4f;

		constexpr float SWIM_MIN_X = -50.2f;
		constexpr float SWIM_MAX_X = 50.2f;
		constexpr float SWIM_MIN_Y = -26.5f;
		constexpr float SWIM_MAX_Y = 26.2f;

		constexpr float DEFAULT_CAMERA_ZOOM = 80.0f;

		constexpr size_t HERO_FISH_COUNT = 16;
		constexpr size_t MINNOW_COUNT = 14;
		constexpr size_t INITIAL_FISH = HERO_FISH_COUNT + MINNOW_COUNT;
		constexpr size_t MAX_FISH = 36;
		constexpr size_t MAX_BUBBLES = 56;
		constexpr size_t MAX_FOOD = 32;
		constexpr size_t VENT_COUNT = 3;

		enum class FishSpecies : uint8_t
		{
			Clownfish = 0,
			BlueTang = 1,
			NeonTetra = 2,
			SunGold = 3
		};

		struct Fish
		{
			FishSpecies species = FishSpecies::Clownfish;
			Entity shapeEntity = INVALID_ENTITY;
			Entity tailEntity = INVALID_ENTITY;
			vec2 velocity = vec2(6.0f, 0.0f);
			vec2 heading = vec2(1.0f, 0.0f);
			float cruiseSpeed = 8.0f;
			float minSpeed = 4.5f;
			float maxSpeed = 13.5f;
			float swimPhase = 0.0f;
			float swimFreq = 7.5f;
			float visualScale = 1.25f;
			float upSign = 1.0f;
			float boostTimer = 0.0f;
		};

		struct Bubble
		{
			bool active = false;
			float riseSpeed = 7.5f;
			float wobbleFreq = 3.5f;
			float wobbleAmp = 2.6f;
			float phase = 0.0f;
			float stuckTimer = 0.0f;
		};

		struct FishFood
		{
			bool active = false;
			float age = 0.0f;
			float driftPhase = 0.0f;
		};

		struct State
		{
			std::array<Material2DHandle, 4> speciesMaterials{};
			Material2DHandle coralPinkMaterial{4};
			Material2DHandle kelpDeepMaterial{7};
			Material2DHandle kelpBrightMaterial{8};
			Material2DHandle bubbleMaterial{13};
			Material2DHandle foodMaterial{14};
			Material2DHandle pearlMaterial{15};
			Material2DHandle glassMaterial{2};

			std::array<Entity, MAX_BUBBLES> bubblePool{};
			std::array<Entity, MAX_FOOD> foodPool{};

			Entity pearlLightEntity = INVALID_ENTITY;
			Entity waterRippleEntity = INVALID_ENTITY;
			ShapeId fishShapeId = 0;

			float cameraZoom = DEFAULT_CAMERA_ZOOM;
			float targetCameraZoom = DEFAULT_CAMERA_ZOOM;

			float rippleTimer = 0.0f;
			float rippleDuration = 0.6f;
			vec2 rippleCenter = vec2(0.0f);

			std::array<float, VENT_COUNT> ventTimers = {0.15f, 0.55f, 0.90f};
			float eatAudioCooldown = 0.0f;
			float bubbleAudioCooldown = 0.0f;

			size_t nextBubbleCursor = 0;
			size_t nextFoodCursor = 0;
			int activeFoodCount = 0;
			int fishCount = 0;
		};

		void stateInitSystem(Registry& registry, ServiceProvider& services);
		void setupEnvironmentSystem(Registry& registry, ServiceProvider& services);
		void setupKelpAndWaterSystem(Registry& registry, ServiceProvider& services);
		void setupLightingSystem(Registry& registry, ServiceProvider& services);
		void setupPoolsAndFishSystem(Registry& registry, ServiceProvider& services);

		void cameraControlSystem(Registry& registry, ServiceProvider& services);
		void interactionSystem(Registry& registry, ServiceProvider& services);
		void updateLightingSystem(Registry& registry, ServiceProvider& services);
		void updateFoodSystem(Registry& registry, ServiceProvider& services);
		void updateFishFlockingSystem(Registry& registry, ServiceProvider& services);
		void updateFishVisualsSystem(Registry& registry, ServiceProvider& services);
		void updateBubblesSystem(Registry& registry, ServiceProvider& services);
	} // namespace AquariumSceneNamespace

	class AquariumScene : public Scene
	{
	public:
		AquariumScene()
		{
			addStartSystem(AquariumSceneNamespace::stateInitSystem);
			addStartSystem(AquariumSceneNamespace::setupEnvironmentSystem);
			addStartSystem(AquariumSceneNamespace::setupKelpAndWaterSystem);
			addStartSystem(AquariumSceneNamespace::setupLightingSystem);
			addStartSystem(AquariumSceneNamespace::setupPoolsAndFishSystem);

			addUpdateSystem(GlobalSystems::sceneControlSystem);
			addUpdateSystem(AquariumSceneNamespace::cameraControlSystem);
			addUpdateSystem(AquariumSceneNamespace::interactionSystem);
			addUpdateSystem(AquariumSceneNamespace::updateLightingSystem);
			addUpdateSystem(AquariumSceneNamespace::updateFoodSystem);
			addUpdateSystem(AquariumSceneNamespace::updateFishFlockingSystem);
			addUpdateSystem(AquariumSceneNamespace::updateFishVisualsSystem);
			addUpdateSystem(AquariumSceneNamespace::updateBubblesSystem);
		}
	};
} // namespace WeirdEngine
