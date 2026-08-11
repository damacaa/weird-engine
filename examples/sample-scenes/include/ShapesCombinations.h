#pragma once

#include <random>

#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class ShapeCombinatiosScene : public Scene2D
{
public:
	ShapeCombinatiosScene() {}

private:
	Entity m_circle = INVALID_ENTITY;
	float m_circleRadious = 0.0f;
	vec2 m_initialMousePositionInWorld;

	std::vector<Entity> m_uiPoints;

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Floor shape
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.5f},
												  {Primitives::SineWave::PERIOD, 2.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = 2,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 0});

		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 20.0f;
		std::uniform_real_distribution<float> distrib(-range, range);

		// Boxes
		{
			std::uniform_real_distribution<float> distribY(0.0f, 5.0f);

			for (int i = 0; i < 0; ++i)
			{
				float x = distrib(gen) + 15.0f;
				float y = -2.0f + distribY(gen);

				services.shapes().addShape({.shapeId = DefaultShapes::BOX,
											.variables = {{Primitives::Box::POS_X, x},
														  {Primitives::Box::POS_Y, y},
														  {Primitives::Box::SIZE_X, 3.0f},
														  {Primitives::Box::SIZE_Y, 5.0f}},
											.material = static_cast<uint16_t>(4 + i),
											.combination = CombinationType::Addition,
											.hasCollision = true,
											.group = 1});
			}
		}

		// Circle
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 7.5f},
												  {Primitives::Circle::RADIUS, 5.0f}},
									.material = 7,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 2});

		// Subtract star
		services.shapes().addShape({.shapeId = DefaultShapes::STAR,
									.variables = {-2.5f + 15.0f, 12.5f, 5.0f, 0.5f, 13.0f, 5.0f},
									.material = 0,
									.combination = CombinationType::SmoothSubtraction,
									.hasCollision = true,
									.group = 2});

		// Cursor circle
		m_circle = services.shapes().addShape(
			{.shapeId = DefaultShapes::CIRCLE,
			 .variables = {{Primitives::Circle::POS_X, 250.0f}, {Primitives::Circle::POS_Y, 10.0f}},
			 .material = 0,
			 .combination = CombinationType::Subtraction,
			 .hasCollision = true,
			 .group = CustomShape::GLOBAL_GROUP});

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 0.0f},
												  {Primitives::Circle::RADIUS, 30.0f}},
									.material = 0,
									.combination = CombinationType::Intersection,
									.hasCollision = true,
									.group = CustomShape::GLOBAL_GROUP});

		for (int i = 0; i < 10; ++i)
		{
			auto ee = registry.createEntity();
			auto& t = registry.addComponent<Transform>(ee);
			t.position = vec3(15.0f, 15.0f, 10.0f);

			auto& ui = registry.addComponent<UIDot>(ee);
			ui.materialId = 4 + (i % 12);

			m_uiPoints.push_back(ee);
		}

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		float delta = services.time().deltaTime();
		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		float x = services.input().getMouseX();
		float y = services.input().getMouseY();

		// Transform mouse coordinates to world space
		vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		if (services.input().getMouseButtonDown(Input::RightClick))
		{
			m_initialMousePositionInWorld = mousePositionInWorld;
		}
		else if (services.input().getGamepadButtonDown(Input::GamepadButton::LeftShoulder))
		{
			float halfWidth = Display::width / 2.0f;
			float halfHeight = Display::height / 2.0f;

			m_initialMousePositionInWorld =
				ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(halfWidth, halfHeight));
		}

		if (services.input().getMouseButton(Input::RightClick))
		{
			vec2 v = mousePositionInWorld - m_initialMousePositionInWorld;
			m_circleRadious = (std::min)(10.0f, length(v));
		}
		else if (services.input().getGamepadButton(Input::GamepadButton::LeftShoulder))
		{
			m_circleRadious = (std::min)(10.0f, m_circleRadious + (10.0f * delta));
		}
		else
		{
			m_circleRadious -= delta * 10.0f * (m_circleRadious + 1.0f);
			m_circleRadious = (std::max)(0.0f, m_circleRadious);
		}

		{
			CustomShape& cs = registry.getComponent<CustomShape>(m_circle);
			cs.parameters[0] = m_initialMousePositionInWorld.x;
			cs.parameters[1] = m_circleRadious <= 0.0f ? -1000.0f : m_initialMousePositionInWorld.y;
			cs.parameters[2] = m_circleRadious;

			registry.setComponentDirty(cs);
		}

		float volume = AudioEngine::getInstance().getAudioData().currentVolume;
		glm::vec2 center = glm::vec2(75.0f, 75.0f); // Screen center X, Y
		float radius = 50.0f - (volume * 50.0f);	// Distance from center
		float speed = 1.0f;							// How fast they rotate
		float spacing =
			2.0f * 3.14f / (static_cast<float>(m_uiPoints.size())); // Gap between each dot along the circle arc

		for (int i = 0; i < m_uiPoints.size(); i++)
		{
			// Calculate angle: Time moves them, 'i' spreads them out
			float angle = (services.time().time() * speed) + (i * spacing);

			float x = center.x + std::cos(angle) * radius;
			float y = center.y + std::sin(angle) * radius;

			auto& t = registry.getComponent<Transform>(m_uiPoints[i]);
			t.position = vec3(x, y, 0.0f);
		}
	}
};
