#pragma once

#include <vector>

#include <weird-engine.h>

namespace ShapeCombinationsNamespace
{
	struct State
	{
		WeirdEngine::Entity circle = WeirdEngine::INVALID_ENTITY;
		float circleRadius = 0.0f;
		WeirdEngine::vec2 initialMousePositionInWorld = WeirdEngine::vec2(0.0f);
	};

	State& getState(WeirdEngine::Registry& registry);

	void stateInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupEnvironmentSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void setupShapesSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void sceneControlSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cursorCircleSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services);
} // namespace ShapeCombinationsNamespace

class ShapeCombinationsScene : public WeirdEngine::Scene2D
{
public:
	ShapeCombinationsScene()
	{
		addStartSystem(ShapeCombinationsNamespace::stateInitSystem);
		addStartSystem(ShapeCombinationsNamespace::setupEnvironmentSystem);
		addStartSystem(ShapeCombinationsNamespace::setupShapesSystem);
		addStartSystem(ShapeCombinationsNamespace::cameraInitSystem);

		addUpdateSystem(ShapeCombinationsNamespace::sceneControlSystem);
		addUpdateSystem(ShapeCombinationsNamespace::cursorCircleSystem);
		addUpdateSystem(ShapeCombinationsNamespace::cameraTrackingSystem);
	}
};

// Backwards compatibility alias for typo in old name
using ShapeCombinatiosScene = ShapeCombinationsScene;
