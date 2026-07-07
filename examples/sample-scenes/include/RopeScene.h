#pragma once

#include <weird-engine.h>

#include "globals.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class RopeScene : public Scene2D
{
public:
	RopeScene(){
	}

private:
	Entity m_star;
	double m_lastSpawnTime = 0.0;
	std::vector<Entity> m_audioEntities;

	std::vector<Entity> balls;

	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		constexpr int numBalls = 60;
		constexpr int rowWidth = 30;
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
			balls.push_back(entity);
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
				spring.entityA = balls[i];
				spring.entityB = balls[i + rowWidth];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			if (notRightEdge) // Right
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = balls[i];
				spring.entityB = balls[i + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			// Shear Springs (Diagonal)
			if (hasRowBelow && notRightEdge) // Bottom-Right
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = balls[i];
				spring.entityB = balls[i + rowWidth + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}

			if (hasRowBelow && notLeftEdge) // Bottom-Left
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = balls[i];
				spring.entityB = balls[i + rowWidth - 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}
		}

		// Fix corners
		ecs.getComponent<RigidBody2D>(balls[0]).isFixed = true;
		ecs.getComponent<RigidBody2D>(balls[0]).isDirty = true;
		if (numBalls >= rowWidth)
		{
			ecs.getComponent<RigidBody2D>(balls[rowWidth - 1]).isFixed = true;
			ecs.getComponent<RigidBody2D>(balls[rowWidth - 1]).isDirty = true;
			ecs.getComponent<RigidBody2D>(balls[rowWidth]).isFixed = true;
			ecs.getComponent<RigidBody2D>(balls[rowWidth]).isDirty = true;
			ecs.getComponent<RigidBody2D>(balls[(2 * rowWidth) - 1]).isFixed = true;
			ecs.getComponent<RigidBody2D>(balls[(2 * rowWidth) - 1]).isDirty = true;
		}

		// Add base shapes (walls, ground, custom)
		float vars0[8] = {1.0f, 0.5f, 1.0f}; // Floor shape
		addShape(DefaultShapes::SINE, vars0, 3);

		float vars1[8] = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f}; // Custom shape
		m_star = addShape(DefaultShapes::STAR, vars1, 3);

		float vars2[8] = {30.5f, 3.5f, 30.0f, 3.0f};
		// addScreenSpaceShape(3, vars2); // UI overlay shape

		float vars3[8] = {15.0f, -98.0f, 15.0f, 100.0f};
		addShape(DefaultShapes::BOX, vars3, 3, CombinationType::Addition);

		{
			Entity text = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(text);
			t.position = vec3(Display::width / 2.0f, 20.0f, 0.0f);
			auto& textRenderer = ecs.addComponent<UITextRenderer>(text);
			textRenderer.text = "Nice rope dude!";
			textRenderer.material = 4;
			textRenderer.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			textRenderer.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		for (int i = 0; i < 8; ++i)
		{
			// float value = AudioEngine::getAudioVisuals().waveform[i];
			Entity e = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(e);
			t.position = vec3((i + 1) * 20, 20.0f, 0.0f);
			auto& ui = ecs.addComponent<UIDot>(e);
			ui.materialId = 4 + (i % 12);

			m_audioEntities.push_back(e);
		}

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

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		// Animate custom shape over time
		{
			// Instead of getSimulation().getSimulationTime(), we can just use getTime() if Scene provides it, or track delta.
			static float animTime = 0.0f;
			animTime += delta;
			auto& cs = ecs.getComponent<CustomShape>(m_star);
			cs.parameters[4] = static_cast<int>(std::floor(animTime)) % 5 + 2;
			cs.parameters[3] = std::sin(3.1416f * animTime);
			cs.isDirty = true;
		}

		auto audioVisualData = AudioEngine::getInstance().getAudioData();

		if (audioVisualData.waveform.size() > 0)
		{
			int skip = audioVisualData.waveform.size() / m_audioEntities.size();
			for (int i = 0; i < m_audioEntities.size(); ++i)
			{
				float value = audioVisualData.waveform[i * skip];
				auto& t = ecs.getComponent<Transform>(m_audioEntities[i]);
				float nextPos = Display::rHeight - ((50.0f * value) + 20.0f);
				// t.position.y += (std::min)(nextPos - t.position.y, 100.0f * delta);
				t.position.y = nextPos;
			}
		}

		if (Input::GetKey(Input::E))
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
			// Duplicate last SDF, add new shape with it
			registerSDF(m_sdfs.back());

			auto& cam = ecs.getComponent<Transform>(m_mainCamera);
			vec2 screen = {Input::GetMouseX(), Input::GetMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			float vars[8] = {world.x, world.y, 5.0f, 7.5f, 1.0f};
			addShape(m_sdfs.size() - 1, vars, 3);
		}

		if (Input::GetKeyDown(Input::K))
		{
			auto components = ecs.getComponentArray<CustomShape>();
			int id = components->getSize() - 1;

			// We shouldn't call getSimulation() here. 
			// We can mark the custom shape entity for destruction or something.
			ecs.destroyEntity(components->getDataAtIdx(id).Owner);
		}
	}

	void onPhysicsStep(Simulation2D& simulation) override
	{
		if (!Input::GetKey(Input::R))
			return;

		auto size = simulation.getSize();

		for (int i = 0; i < size; i++)
		{
			auto p = simulation.getPosition(i);

			if (p.x > 29.5f)
			{
				p.x = 29.5f;
				simulation.setPosition(i, p);
			}
			else if (p.x < 0.5f)
			{
				p.x = 0.5f;
				simulation.setPosition(i, p);
			}
			else if (p.y < 0.5f)
			{
				p.y = 0.5f;
				simulation.setPosition(i, p);
			}
		}
	}
};
