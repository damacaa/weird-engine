#include "CollisionHandling.h"

#include "globals.h"
#include <cstdio>

using namespace WeirdEngine;

namespace CollisionHandlingNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "CollisionHandling State is missing: stateInitSystem must run first");
		return *state;
	}

	void spawnLaneBalls(Registry& registry, ServiceProvider& services, State& state)
	{
		// Clean up previous balls
		for (Entity e : state.activeBalls)
		{
			if (registry.isEntityValid(e))
			{
				registry.destroyEntity(e);
			}
		}
		state.activeBalls.clear();

		// Lane 1: Main Thread ECS Events (x: -18 to -8)
		constexpr int LANE1_BALLS = 12;
		for (int i = 0; i < LANE1_BALLS; ++i)
		{
			Entity e = registry.createEntity();
			auto& t = registry.addComponent<Transform>(e);
			t.position =
				vec3(-16.0f + static_cast<float>(i % 3) * 2.5f, 22.0f + static_cast<float>(i / 3) * 3.5f, 0.0f);

			auto& dot = registry.addComponent<Dot>(e);
			dot.materialId = state.ballMat.id;

			auto& rb = registry.addComponent<RigidBody2D>(e);
			rb.isFixed = false;

			state.activeBalls.push_back(e);
		}

		// Lane 2: Physics Thread Shape Collisions (Launch Pad) (x: 2 to 10)
		constexpr int LANE2_BALLS = 8;
		for (int i = 0; i < LANE2_BALLS; ++i)
		{
			Entity e = registry.createEntity();
			auto& t = registry.addComponent<Transform>(e);
			t.position = vec3(4.0f + static_cast<float>(i % 2) * 3.0f, 20.0f + static_cast<float>(i / 2) * 4.0f, 0.0f);

			auto& dot = registry.addComponent<Dot>(e);
			dot.materialId = state.ballMat.id;

			auto& rb = registry.addComponent<RigidBody2D>(e);
			rb.isFixed = false;

			state.activeBalls.push_back(e);
		}

		// Lane 3: Physics Thread Body Collisions (Elastic Bouncers) (x: 20 to 32)
		constexpr int LANE3_BALLS = 10;
		for (int i = 0; i < LANE3_BALLS; ++i)
		{
			Entity e = registry.createEntity();
			auto& t = registry.addComponent<Transform>(e);
			t.position = vec3(22.0f + static_cast<float>(i % 2) * 4.0f, 18.0f + static_cast<float>(i / 2) * 4.0f, 0.0f);

			auto& dot = registry.addComponent<Dot>(e);
			dot.materialId = state.ballMat.id;

			auto& rb = registry.addComponent<RigidBody2D>(e);
			rb.isFixed = false;

			state.activeBalls.push_back(e);
		}
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		services.physics().setGravity(-9.8f);
		services.physics().setDamping(0.001f);

		// Set Camera framing the 3 lanes nicely
		registry.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(7.0f, 15.0f, 40.0f);
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Materials
		auto& ballMat = services.materials2D().createMaterial("lane_ball");
		ballMat.color = ColorPalette::LightBlue;
		state.ballMat = ballMat.id;

		auto& hitMat = services.materials2D().createMaterial("lane_hit");
		hitMat.color = ColorPalette::Orange;
		hitMat.emission = 0.5f;
		state.hitMat = hitMat.id;

		auto& barrierMat = services.materials2D().createMaterial("lane_barrier");
		barrierMat.color = ColorPalette::LightGray;
		state.barrierMat = barrierMat.id;

		auto& padMat = services.materials2D().createMaterial("launch_pad");
		padMat.color = ColorPalette::LightGreen;
		padMat.emission = 0.4f;

		auto& floorMat = services.materials2D().createMaterial("main_floor");
		floorMat.color = ColorPalette::DarkGray;

		auto& textMat = services.materials2D().createMaterial("ch_text");
		textMat.color = ColorPalette::White;
	}

	void setupArenaShapesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& floorMat = services.materials2D().get("main_floor");
		auto& padMat = services.materials2D().get("launch_pad");

		// ------------------------------------------------------- Lane Dividers & Shapes
		// Main ground floor
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.0f},
												  {Primitives::SineWave::PERIOD, 0.0f},
												  {Primitives::SineWave::SPEED, 0.0f},
												  {Primitives::SineWave::OFFSET, 0.0f}},
									.material = floorMat,
									.combination = CombinationType::Addition});

		// Left boundary wall
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, -22.0f},
												  {Primitives::Box::POS_Y, 15.0f},
												  {Primitives::Box::SIZE_X, 2.0f},
												  {Primitives::Box::SIZE_Y, 25.0f}},
									.material = state.barrierMat});

		// Divider 1 (between Lane 1 and Lane 2)
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, -4.0f},
												  {Primitives::Box::POS_Y, 12.0f},
												  {Primitives::Box::SIZE_X, 1.5f},
												  {Primitives::Box::SIZE_Y, 20.0f}},
									.material = state.barrierMat});

		// Divider 2 (between Lane 2 and Lane 3)
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 15.0f},
												  {Primitives::Box::POS_Y, 12.0f},
												  {Primitives::Box::SIZE_X, 1.5f},
												  {Primitives::Box::SIZE_Y, 20.0f}},
									.material = state.barrierMat});

		// Right boundary wall
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 36.0f},
												  {Primitives::Box::POS_Y, 15.0f},
												  {Primitives::Box::SIZE_X, 2.0f},
												  {Primitives::Box::SIZE_Y, 25.0f}},
									.material = state.barrierMat});

		// Lane 1 Obstacle: Pegs for ECS collision triggers
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, -13.0f},
												  {Primitives::Circle::POS_Y, 10.0f},
												  {Primitives::Circle::RADIUS, 1.5f}},
									.material = state.barrierMat});

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, -8.0f},
												  {Primitives::Circle::POS_Y, 6.0f},
												  {Primitives::Circle::RADIUS, 1.5f}},
									.material = state.barrierMat});

		// Lane 2 Obstacle: Launch Pad (trigger zone for physics-thread shape collision)
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 6.0f},
												  {Primitives::Box::POS_Y, 0.0f},
												  {Primitives::Box::SIZE_X, 4.0f},
												  {Primitives::Box::SIZE_Y, 1.0f}},
									.material = padMat});

		// Lane 3 Obstacle: Angled ramps to funnel balls toward each other for body collisions
		services.shapes().addShape({.shapeId = DefaultShapes::TRIANGLE,
									.variables = {{DefaultShapes::Triangle::POS_X, 18.0f},
												  {DefaultShapes::Triangle::POS_Y, 6.0f},
												  {DefaultShapes::Triangle::WIDTH, 5.0f},
												  {DefaultShapes::Triangle::HEIGHT, 5.0f}},
									.material = state.barrierMat});
	}

	void setupHudSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& textMat = services.materials2D().get("ch_text");

		// HUD Text for live stats
		state.hudText = registry.createEntity();
		auto& ht = registry.addComponent<Transform>(state.hudText);
		ht.position = vec3(20.0f, 20.0f, 0.0f);

		auto& tr = registry.addComponent<UITextRenderer>(state.hudText);
		tr.text = "COLLISION HANDLING OPTIONS | ECS Impacts: 0 | [Space]: Respawn | [Q]: Next";
		tr.material = textMat.id;
		tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
	}

	void spawnInitialBallsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		spawnLaneBalls(registry, services, state);
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void spawnerSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Periodic or manual respawn
		state.respawnTimer += services.time().deltaTime();
		if (state.respawnTimer > 7.0f || services.input().getKeyDown(Input::Space))
		{
			state.respawnTimer = 0.0f;
			spawnLaneBalls(registry, services, state);
		}
	}

	void hudSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Update HUD text with live collision counts
		char buf[128];
		if (registry.isEntityValid(state.hudText) && registry.hasComponent<UITextRenderer>(state.hudText))
		{
			auto& tr = registry.getComponent<UITextRenderer>(state.hudText);
			std::snprintf(buf, sizeof(buf),
						  "COLLISION HANDLING OPTIONS | ECS Impacts: %d | [Space]: Respawn | [Q]: Next",
						  state.ecsCollisionCount);
			tr.text = buf;
			registry.setComponentDirty(tr);
		}
	}

	void onEntityShapeCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		State& state = getState(registry);

		if (event.entity != INVALID_ENTITY && registry.hasComponent<Dot>(event.entity))
		{
			state.ecsCollisionCount++;

			// Flash ball material on impact
			auto& dot = registry.getComponent<Dot>(event.entity);
			dot.materialId = state.hitMat.id;
			registry.setComponentDirty(dot);

			// Play impact audio
			services.audio().playSound({0.03f, 500.0f, true, vec3(event.raw.position, 0.0f), 1});
		}
	}

	void onEntityCollisionSystem(Registry& registry, ServiceProvider& services, EntityCollisionEvent& event)
	{
		State& state = getState(registry);
		state.ecsCollisionCount++;

		if (event.entityA != INVALID_ENTITY && registry.hasComponent<Dot>(event.entityA))
		{
			auto& dot = registry.getComponent<Dot>(event.entityA);
			dot.materialId = state.hitMat.id;
			registry.setComponentDirty(dot);
		}

		services.audio().playSound({0.02f, 750.0f, true, vec3(event.raw.position, 0.0f), 1});
	}

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace CollisionHandlingNamespace

void CollisionHandlingScene::onPhysicsShapeCollision(Simulation2D& simulation, PhysicsShapeCollisionEvent& event)
{
	vec2 pos = simulation.getPosition(event.body);

	// Lane 2 Launch Pad: if body is hitting near pad X: [4..8], Y: [-2..1], apply strong launch impulse
	if (pos.x >= 4.0f && pos.x <= 8.0f && pos.y <= 1.5f)
	{
		simulation.addImpulseForce(event.body, vec2(0.0f, 25.0f));
	}
}

void CollisionHandlingScene::onPhysicsRigidBodyCollision(Simulation2D& simulation, PhysicsCollisionEvent& event)
{
	vec2 posA = simulation.getPosition(event.bodyA);
	vec2 posB = simulation.getPosition(event.bodyB);

	// Lane 3 Elastic Bouncers: boost repulsion between balls in Lane 3 (X > 16)
	if (posA.x > 16.0f && posB.x > 16.0f)
	{
		vec2 diff = posA - posB;
		float d = glm::length(diff);
		if (d > 0.001f)
		{
			vec2 dir = diff / d;
			simulation.addImpulseForce(event.bodyA, dir * 8.0f);
			simulation.addImpulseForce(event.bodyB, -dir * 8.0f);
		}
	}
}