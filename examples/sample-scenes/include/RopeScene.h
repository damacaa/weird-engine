#pragma once

#include <stack>
#include <vector>

#include "GlobalSystems.h"
#include <weird-engine.h>
#include <weird-engine/FixedVector.h>

namespace RopeSceneNamespace
{
	struct RopeBall
	{
		bool shouldClamp = false;
	};

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

		std::array<WeirdEngine::Material2DHandle, 8> ballMats{};

		bool clampBalls = false;

		WeirdEngine::Entity startRopeBall = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::Entity lastSelectedBall = WeirdEngine::INVALID_ENTITY;
		WeirdEngine::FixedVector<WeirdEngine::Entity, 64> currentRopeBalls;
		WeirdEngine::FixedVector<WeirdEngine::Entity, 64> currentRopeSprings;
		bool creatingRope = false;
		WeirdEngine::vec2 startRopeMousePos = WeirdEngine::vec2(0.0f);

		WeirdEngine::FixedVector<WeirdEngine::Entity, WeirdEngine::MAX_ENTITIES.id> toDeleteRopeBalls;
		WeirdEngine::FixedVector<WeirdEngine::Entity, WeirdEngine::MAX_ENTITIES.id> toDeleteRopeSprings;
		float deleteRopeBallsTimer = 0.0f;

		WeirdEngine::Entity draggedBall = WeirdEngine::INVALID_ENTITY;

		WeirdEngine::vec2 boxStart{0.0f, 0.0f};
	};

	static_assert(WeirdEngine::is_snapshot_serializable_v<State>);
	static_assert(WeirdEngine::is_snapshot_serializable_v<RopeBall>);

	State& getState(WeirdEngine::Registry& registry);
	void throwBalls(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services, State& state);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupMaterialsSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupRopeGridSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void starAnimationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void spawnerSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void clampToggleSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ropeCreationSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cancelRopeCreation(WeirdEngine::Registry& registry, State& state);
	void ballDragSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void ropeCleanupSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void fallCleanupSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
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

		addUpdateSystem(GlobalSystems::sceneControlSystem);
		addUpdateSystem(RopeSceneNamespace::starAnimationSystem);
		addUpdateSystem(RopeSceneNamespace::spawnerSystem);
		addUpdateSystem(RopeSceneNamespace::clampToggleSystem);
		addUpdateSystem(RopeSceneNamespace::ropeCreationSystem);
		addUpdateSystem(RopeSceneNamespace::ballDragSystem);
		addUpdateSystem(RopeSceneNamespace::ropeCleanupSystem);
		addUpdateSystem(RopeSceneNamespace::fallCleanupSystem);
	}

	void onPhysicsStep(WeirdEngine::Simulation2D& simulation) override;
	void onSnapshotLoaded(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services) override;
};
