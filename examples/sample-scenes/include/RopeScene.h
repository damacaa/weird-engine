#pragma once

#include <weird-engine.h>

#include "globals.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class RopeScene : public Scene2D
{
public:
	RopeScene() {}

private:
	Entity m_star = INVALID_ENTITY;
	double m_lastSpawnTime = 0.0;

	std::vector<Entity> m_balls;

	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		constexpr int rowWidth = 30;
		constexpr int numBalls = rowWidth * 2;
		constexpr float startY = 20.0f + (numBalls / rowWidth);
		constexpr float stiffness = 1.0f;

		// Create 2D rigid bodies in a rope/grid layout
		for (int i = 0; i < numBalls; ++i)
		{
			float x = static_cast<float>(i % rowWidth);
			float y = startY - static_cast<float>(i / rowWidth);
			int material = 4 + (i % 12);

			Entity entity = ecs.createEntity();

			auto& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0.0f);

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = material;

			auto& rb = ecs.addComponent<RigidBody2D>(entity);
			m_balls.push_back(entity);
		}

		// Connect balls with springs (down and right)
		for (int i = 0; i < numBalls; ++i)
		{
			bool hasRowBelow = (i + rowWidth < numBalls);
			bool notRightEdge = ((i + 1) % rowWidth != 0);
			bool notLeftEdge = (i % rowWidth != 0);

			// Structural Springs (Down and Right)
			if (hasRowBelow) // Down
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			if (notRightEdge) // Right
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			// Shear Springs (Diagonal)
			if (hasRowBelow && notRightEdge) // Bottom-Right
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}

			if (hasRowBelow && notLeftEdge) // Bottom-Left
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth - 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}
		}

		// Fix corners
		ecs.getComponent<RigidBody2D>(m_balls[0]).isFixed = true;
		ecs.setEntityDirty<RigidBody2D>(m_balls[0], true);
		if (numBalls > rowWidth)
		{
			ecs.getComponent<RigidBody2D>(m_balls[rowWidth - 1]).isFixed = true;
			ecs.setEntityDirty<RigidBody2D>(m_balls[rowWidth - 1], true);
			ecs.getComponent<RigidBody2D>(m_balls[rowWidth]).isFixed = true;
			ecs.setEntityDirty<RigidBody2D>(m_balls[rowWidth], true);
			ecs.getComponent<RigidBody2D>(m_balls[(2 * rowWidth) - 1]).isFixed = true;
			ecs.setEntityDirty<RigidBody2D>(m_balls[(2 * rowWidth) - 1], true);
		}

		// Add base shapes (walls, ground, custom)
		float vars0[8] = {1.0f, 0.5f, 1.0f}; // Floor shape
		addShape(DefaultShapes::SINE, vars0, 3);

		float vars1[8] = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f}; // Custom shape
		// m_star = addShape(DefaultShapes::STAR, vars1, 3);

		float vars3[8] = {15.0f, -98.0f, 15.0f, 100.0f};
		addShape(DefaultShapes::BOX, vars3, 3, CombinationType::Addition);

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
	}

	void throwBalls(ECSManager& ecs)
	{
		if (getTime() <= m_lastSpawnTime + 0.1)
		{
			return;
		}

		constexpr int amount = 10;
		for (int i = 0; i < amount; ++i)
		{
			float y = 60.0f + (1.2f * i);

			Entity entity = ecs.createEntity();

			auto& t = ecs.addComponent<Transform>(entity);
			t.position = vec3(0.5f, y + 0.5f, 0.0f);

			auto& sdf = ecs.addComponent<Dot>(entity);
			sdf.materialId = 4 + ecs.getComponentArray<Dot>()->getSize() % 12;

			auto& rb = ecs.addComponent<RigidBody2D>(entity);
			rb.pendingImpulseForce += vec2(20.0f, 0.0f);
		}

		m_lastSpawnTime = getTime();
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}

		// Animate custom shape over time
		if (m_star != INVALID_ENTITY)
		{
			// Instead of getSimulation().getSimulationTime(), we can just use getTime() if Scene provides it, or track
			// delta.
			static float animTime = 0.0f;
			animTime += delta;
			auto& cs = ecs.getComponent<CustomShape>(m_star);
			cs.parameters[4] = static_cast<int>(std::floor(animTime)) % 5 + 2;
			cs.parameters[3] = std::sin(3.1416f * animTime);
			ecs.setComponentDirty(cs);
		}

		if (Input::GetKey(Input::E) || Input::GetGamepadButton(Input::GamepadButton::West))
		{
			throwBalls(ecs);
		}

		static vec2 boxStart;
		static bool createBoxInUI = true;
		if (Input::GetKeyDown(Input::M))
		{
			auto& cam = ecs.getComponent<Transform>(m_mainCamera);
			vec2 screen = {Input::GetMouseX(), Input::GetMouseY()};

			if (createBoxInUI)
			{
				boxStart = vec2(screen.x, screen.y);
			}
			else
			{
				vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);
				boxStart = world;
			}
		}
		else if (Input::GetKeyUp(Input::M))
		{
			auto& cam = ecs.getComponent<Transform>(m_mainCamera);
			vec2 screen = {Input::GetMouseX(), Input::GetMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			vec2 boxEnd;
			if (createBoxInUI)
			{
				boxEnd = vec2(screen.x, screen.y);
			}
			else
			{
				boxEnd = world;
			}

			float x = (boxStart.x + boxEnd.x) / 2.0f;
			float y = (boxStart.y + boxEnd.y) / 2.0f;
			float w = 0.5f * std::abs(boxStart.x - boxEnd.x);
			float h = 0.5f * std::abs(boxStart.y - boxEnd.y);
			float vars[8] = {x, y, w, h, 1.2f};

			if (createBoxInUI)
				addUIShape(DefaultShapes::BOX, vars, 7, CombinationType::SmoothAddition);
			else
				addShape(DefaultShapes::BOX, vars, 4 + ecs.getComponentArray<CustomShape>()->getSize() % 12,
						 CombinationType::SmoothAddition, true, ecs.getComponentArray<CustomShape>()->getSize());
		}

		if (Input::GetKeyDown(Input::N))
		{
			auto& cam = ecs.getComponent<Transform>(m_mainCamera);
			vec2 screen = {Input::GetMouseX(), Input::GetMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			float vars[8] = {world.x, world.y, 5.0f, 7.5f, 1.0f};
			addShape(DefaultShapes::STAR, vars, 3);
		}

		if (Input::GetKey(Input::R) || Input::GetGamepadButton(Input::GamepadButton::South))
		{
			ecs.forEach<RigidBody2D, Transform>(
				[&](Entity e, RigidBody2D& rb, Transform& t)
				{
					vec2 force(0, -0.001f * (t.position.y * t.position.y));
					force.x += t.position.x < 0.0f ? -t.position.x : 0.0f;
					force.x -= t.position.x > 30.0f ? t.position.x - 30.0f : 0.0f;
					force.x = 10.0f / delta * glm::clamp(force.x, -1.0f, 1.0f);

					rb.pendingContinuousForce = force;
				});
		}
	}
};
