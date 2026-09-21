#pragma once

#include <stack>
#include <vector>

#include <weird-engine.h>

namespace RopeSceneNamespace
{
	struct BallData : WeirdEngine::BodyUserData
	{
		static constexpr int TYPE = 1;

		BallData()
		{
			type = TYPE;
		}

		bool shouldClamp = false;
	};

	struct State
	{
		WeirdEngine::Entity star = WeirdEngine::INVALID_ENTITY;
		double lastSpawnTime = 0.0;
		float animTime = 0.0f;

		std::vector<WeirdEngine::Entity> balls;
		std::vector<WeirdEngine::Material2DHandle> ballMats;

		bool clampBalls = false;

		WeirdEngine::Entity startRopeBall = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity lastSelectedBall = WeirdEngine::INVALID_ENTITY;
		std::vector<WeirdEngine::Entity> currentRopeBalls;
		std::vector<WeirdEngine::Entity> currentRopeSprings;
		bool creatingRope = false;
		WeirdEngine::vec2 startRopeMousePos = WeirdEngine::vec2(0.0f);

		std::stack<WeirdEngine::Entity> toDeleteRopeBalls;
		std::stack<WeirdEngine::Entity> toDeleteRopeSprings;
		float deleteRopeBallsTimer = 0.0f;

		WeirdEngine::Entity draggedBall = WeirdEngine::INVALID_ENTITY;
	};

	State& getState(WeirdEngine::Registry& registry);
	void throwBalls(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, State& state);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupRopeGridSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void starAnimationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void clampToggleSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ropeCreationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ballDragSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ropeCleanupSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void fallCleanupSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace RopeSceneNamespace

class RopeScene : public WeirdEngine::Scene2D
{
public:
	RopeScene()
	{
		addStartSystem(RopeSceneNamespace::stateInitSystem);
		addStartSystem(RopeSceneNamespace::setupEnvironmentSystem);
		addStartSystem(RopeSceneNamespace::setupMaterialsSystem);
		addStartSystem(RopeSceneNamespace::setupRopeGridSystem);
		addStartSystem(RopeSceneNamespace::setupShapesSystem);
		addStartSystem(RopeSceneNamespace::cameraInitSystem);

		addUpdateSystem(RopeSceneNamespace::sceneControlSystem);
		addUpdateSystem(RopeSceneNamespace::starAnimationSystem);
		addUpdateSystem(RopeSceneNamespace::spawnerSystem);
		addUpdateSystem(RopeSceneNamespace::clampToggleSystem);
		addUpdateSystem(RopeSceneNamespace::ropeCreationSystem);
		addUpdateSystem(RopeSceneNamespace::ballDragSystem);
		addUpdateSystem(RopeSceneNamespace::ropeCleanupSystem);
		addUpdateSystem(RopeSceneNamespace::fallCleanupSystem);
		addUpdateSystem(RopeSceneNamespace::cameraTrackingSystem);
	}

	void onPhysicsStep(WeirdEngine::Simulation2D& simulation) override;
};
