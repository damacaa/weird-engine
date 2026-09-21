#include "ShapesCombinations.h"

#include "globals.h"
#include <algorithm>
#include <cmath>
#include <random>

using namespace WeirdEngine;

namespace ShapeCombinationsNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "ShapeCombinations State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);
	}

	void setupShapesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& floorMat = services.materials2D().createMaterial("floor");
		floorMat.color = ColorPalette::Gray;

		auto& circleMat = services.materials2D().createMaterial("circle");
		circleMat.color = vec4(ColorPalette::Yellow, 0.25f);

		auto& voidMat = services.materials2D().createMaterial("void");
		voidMat.color = ColorPalette::Black;

		// Floor shape
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.5f},
												  {Primitives::SineWave::PERIOD, 2.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = floorMat,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 0});

		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 20.0f;
		std::uniform_real_distribution<float> distrib(-range, range);

		// Circle
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 7.5f},
												  {Primitives::Circle::RADIUS, 5.0f}},
									.material = circleMat,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 2});

		// Subtract star
		services.shapes().addShape({.shapeId = DefaultShapes::STAR,
									.variables = {-2.5f + 15.0f, 12.5f, 5.0f, 0.5f, 13.0f, 5.0f},
									.material = voidMat,
									.combination = CombinationType::SmoothSubtraction,
									.hasCollision = true,
									.group = 2});

		// Cursor circle
		state.circle = services.shapes().addShape(
			{.shapeId = DefaultShapes::CIRCLE,
			 .variables = {{Primitives::Circle::POS_X, 250.0f}, {Primitives::Circle::POS_Y, 10.0f}},
			 .material = voidMat,
			 .combination = CombinationType::Subtraction,
			 .hasCollision = true,
			 .group = Shape::GLOBAL_GROUP});

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 0.0f},
												  {Primitives::Circle::RADIUS, 30.0f}},
									.material = voidMat,
									.combination = CombinationType::Intersection,
									.hasCollision = true,
									.group = Shape::GLOBAL_GROUP});
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void cursorCircleSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		float x = services.input().getMouseX();
		float y = services.input().getMouseY();

		// Transform mouse coordinates to world space
		vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		if (services.input().getMouseButtonDown(Input::RightClick))
		{
			state.initialMousePositionInWorld = mousePositionInWorld;
		}
		else if (services.input().getGamepadButtonDown(Input::GamepadButton::LeftShoulder))
		{
			float halfWidth = Display::width / 2.0f;
			float halfHeight = Display::height / 2.0f;

			state.initialMousePositionInWorld =
				ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(halfWidth, halfHeight));
		}

		if (services.input().getMouseButton(Input::RightClick))
		{
			vec2 v = mousePositionInWorld - state.initialMousePositionInWorld;
			state.circleRadius = (std::min)(10.0f, length(v));
		}
		else if (services.input().getGamepadButton(Input::GamepadButton::LeftShoulder))
		{
			state.circleRadius = (std::min)(10.0f, state.circleRadius + (10.0f * delta));
		}
		else
		{
			state.circleRadius -= delta * 10.0f * (state.circleRadius + 1.0f);
			state.circleRadius = (std::max)(0.0f, state.circleRadius);
		}

		if (registry.isEntityValid(state.circle) && registry.hasComponent<Shape>(state.circle))
		{
			Shape& cs = registry.getComponent<Shape>(state.circle);
			cs.parameters[0] = state.initialMousePositionInWorld.x;
			cs.parameters[1] = state.circleRadius <= 0.0f ? -1000.0f : state.initialMousePositionInWorld.y;
			cs.parameters[2] = state.circleRadius;

			registry.setComponentDirty(cs);
		}
	}

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace ShapeCombinationsNamespace