#pragma once

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

#include "globals.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;

struct BallData : BodyUserData
{
	static constexpr int TYPE = 1;

	BallData()
	{
		type = TYPE;
	}

	bool shouldClamp = false;
};

// Example scene demonstrating how to create a rope of connected circles using springs.
class RopeScene : public Scene2D
{
public:
	RopeScene() {}

	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		// Local coordinate p centered at (0, 0)
		Vec2Expr p = SDF::point();

		// Taut string curve: sinusoidal wave modulated by box (scaled 10x for UI)
		Expr wave = sdSineWave(p, 20.0f, 0.05f, 0.8f, 0.0f);
		Expr box = sdBox(p, Vec2Expr(30.0f, 8.0f));
		Expr ropeShape = sdfSmoothUnion(wave, box, 4.0f);

		return WeirdAudio::SdfSong::create("rope", ropeShape);
	}

private:
	Entity m_star = INVALID_ENTITY;
	double m_lastSpawnTime = 0.0;

	std::vector<Entity> m_balls;
	std::vector<Material2DHandle> m_ballMats;

	bool m_clampBalls = false;

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio with scene-defined rope song
		services.audio().setSong(createSceneSong());

		auto& groundMat = services.materials2D().createMaterial("ground");
		groundMat.color = ColorPalette::LightGray;

		auto& uiBoxMat = services.materials2D().createMaterial("ui_box");
		uiBoxMat.color = ColorPalette::Yellow;

		for (int i = 0; i < 8; ++i)
		{
			auto& mat = services.materials2D().createMaterial("ball_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			m_ballMats.push_back(mat.id);
		}

		constexpr int rowWidth = 30;
		constexpr int numBalls = rowWidth * 2;
		constexpr float startY = 20.0f + (numBalls / rowWidth);
		constexpr float stiffness = 1.0f;

		// Create 2D rigid bodies in a rope/grid layout
		for (int i = 0; i < numBalls; ++i)
		{
			float x = static_cast<float>(i % rowWidth);
			float y = startY - static_cast<float>(i / rowWidth);

			Entity entity = registry.createEntity();

			auto& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0.0f);

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = m_ballMats[i % m_ballMats.size()].id;

			auto& rb = registry.addComponent<RigidBody2D>(entity);
			services.physics().setUserData(rb.simulationId, std::make_unique<BallData>());
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
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			if (notRightEdge) // Right
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			// Shear Springs (Diagonal)
			if (hasRowBelow && notRightEdge) // Bottom-Right
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}

			if (hasRowBelow && notLeftEdge) // Bottom-Left
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = m_balls[i];
				spring.entityB = m_balls[i + rowWidth - 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}
		}

		// Fix corners
		registry.getComponent<RigidBody2D>(m_balls[0]).isFixed = true;
		registry.setEntityDirty<RigidBody2D>(m_balls[0], true);
		if (numBalls > rowWidth)
		{
			registry.getComponent<RigidBody2D>(m_balls[rowWidth - 1]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(m_balls[rowWidth - 1], true);
			registry.getComponent<RigidBody2D>(m_balls[rowWidth]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(m_balls[rowWidth], true);
			registry.getComponent<RigidBody2D>(m_balls[(2 * rowWidth) - 1]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(m_balls[(2 * rowWidth) - 1], true);
		}

		// Add base shapes (walls, ground, custom)
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 1.0f},
												  {Primitives::SineWave::PERIOD, 0.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = groundMat});

		m_star = services.shapes().addShape({.shapeId = DefaultShapes::STAR,
											 .variables = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f},
											 .material = groundMat});

		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 15.0f},
												  {Primitives::Box::POS_Y, -98.0f},
												  {Primitives::Box::SIZE_X, 15.0f},
												  {Primitives::Box::SIZE_Y, 100.0f}},
									.material = groundMat,
									.combination = CombinationType::Addition});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	void throwBalls(Registry& registry, ServiceProvider& services)
	{
		if (services.time().time() <= m_lastSpawnTime + 0.1)
		{
			return;
		}

		services.audio().playSound({0.015f, 150.0f + (std::rand() % 150), true, vec3(0.0f), 1});

		constexpr int amount = 10;
		for (int i = 0; i < amount; ++i)
		{
			float y = 60.0f + (1.2f * i);

			Entity entity = registry.createEntity();

			auto& t = registry.addComponent<Transform>(entity);
			t.position = vec3(2.5f, y + 0.5f, 0.0f);

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = m_ballMats[registry.getComponentArray<Dot>()->getSize() % m_ballMats.size()].id;

			auto& rb = registry.addComponent<RigidBody2D>(entity);
			auto ballData = std::make_unique<BallData>();
			ballData->shouldClamp = m_clampBalls;
			services.physics().setUserData(rb.simulationId, std::move(ballData));
			rb.pendingImpulseForce += vec2(20.0f, 0.0f);
		}

		m_lastSpawnTime = services.time().time();
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		float delta = services.time().deltaTime();
		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		// Animate custom shape over time
		if (m_star != INVALID_ENTITY)
		{
			// Instead of getSimulation().getSimulationTime(), we can just use services.time().time() if Scene provides
			// it, or track delta.
			static float animTime = 0.0f;
			animTime += delta;
			auto& cs = registry.getComponent<CustomShape>(m_star);
			cs.parameters[4] = static_cast<float>((static_cast<int>(std::floor(animTime)) % 5) + 2);
			cs.parameters[3] = std::sin(3.1416f * animTime);
			registry.setComponentDirty(cs);
		}

		if (services.input().getKey(Input::E) || services.input().getGamepadButton(Input::GamepadButton::West))
		{
			throwBalls(registry, services);
		}

		if (services.input().getKeyDown(Input::N))
		{
			auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			services.shapes().addShape({.shapeId = DefaultShapes::STAR,
										.variables = {world.x, world.y, 5.0f, 7.5f, 1.0f},
										.material = services.materials2D().getHandle("ground")});
		}

		if (services.input().getKeyDown(Input::R) || services.input().getGamepadButtonDown(Input::GamepadButton::South))
		{
			m_clampBalls = !m_clampBalls;
			services.physics().forEachUserData(
				[&](SimulationID id, BodyUserData& data)
				{
					if (data.type == BallData::TYPE)
					{
						auto& ballData = static_cast<BallData&>(data);
						ballData.shouldClamp = m_clampBalls;
					}
				});
		}
	}

	void onPhysicsStep(Simulation2D& simulation) override
	{
		float dt = static_cast<float>(simulation.getDeltaTime());

		simulation.forEachUserData(
			[&](SimulationID id, BodyUserData& data)
			{
				if (data.type == BallData::TYPE)
				{
					auto& ballData = static_cast<BallData&>(data);
					if (ballData.shouldClamp)
					{
						vec2 pos = simulation.getPhysicsPosition(id);

						vec2 force(0.0f, 0.0f);
						force.x += pos.x < 2.5f ? -pos.x + 2.5f : 0.0f;
						force.x -= pos.x > 27.5f ? pos.x - 27.5f : 0.0f;
						force.x = (10.0f / dt) * glm::clamp(force.x, -1.0f, 1.0f);
						force.y += pos.y < 0.0f ? -pos.y : 0.0f;

						simulation.addImpulseForce(id, force * dt);
					}
				}
			});
	}
};
