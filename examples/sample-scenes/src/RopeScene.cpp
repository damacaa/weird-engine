#include "RopeScene.h"

#include "globals.h"
#include <cmath>

using namespace WeirdEngine;

namespace RopeSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "RopeScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void throwBalls(Registry& registry, ServiceProvider& services, State& state)
	{
		if (services.time().time() <= state.lastSpawnTime + 0.1)
		{
			return;
		}

		if (registry.getEntityCount() >= MAX_ENTITIES - 50)
		{
			return;
		}

		services.audio().playSound({0.015f, 150.0f + (std::rand() % 150), true, vec3(0.0f), 1});

		constexpr int amount = 10;
		for (int i = 0; i < amount; ++i)
		{
			float y = 60.0f + (1.2f * i);

			Entity entity = registry.createEntity();
			if (entity == INVALID_ENTITY)
			{
				break;
			}

			auto& t = registry.addComponent<Transform>(entity);
			t.position = vec3(2.5f, y + 0.5f, 0.0f);

			auto& sdf = registry.addComponent<Dot>(entity);
			sdf.materialId = state.ballMats[registry.getComponentArray<Dot>()->getSize() % state.ballMats.size()].id;

			auto& rb = registry.addComponent<RigidBody2D>(entity);
			rb.isFixed = false;
			auto ballData = std::make_unique<BallData>();
			ballData->shouldClamp = state.clampBalls;
			services.physics().setUserData(rb.simulationId, std::move(ballData));
			rb.pendingImpulseForce += vec2(20.0f, 0.0f);
		}

		state.lastSpawnTime = services.time().time();
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		services.debug().setDebugInput(false);
		services.debug().setDebugFly(true);
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& groundMat = services.materials2D().createMaterial("ground");
		groundMat.color = ColorPalette::LightGray;

		auto& uiBoxMat = services.materials2D().createMaterial("ui_box");
		uiBoxMat.color = ColorPalette::Yellow;

		for (int i = 0; i < 8; ++i)
		{
			auto& mat = services.materials2D().createMaterial("ball_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			state.ballMats.push_back(mat.id);
		}
	}

	void setupRopeGridSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

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
			sdf.materialId = state.ballMats[i % state.ballMats.size()].id;

			auto& rb = registry.addComponent<RigidBody2D>(entity);
			services.physics().setUserData(rb.simulationId, std::make_unique<BallData>());
			state.balls.push_back(entity);
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
				spring.entityA = state.balls[i];
				spring.entityB = state.balls[i + rowWidth];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			if (notRightEdge) // Right
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = state.balls[i];
				spring.entityB = state.balls[i + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.0f;
			}

			// Shear Springs (Diagonal)
			if (hasRowBelow && notRightEdge) // Bottom-Right
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = state.balls[i];
				spring.entityB = state.balls[i + rowWidth + 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}

			if (hasRowBelow && notLeftEdge) // Bottom-Left
			{
				Entity springEnt = registry.createEntity();
				auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
				spring.entityA = state.balls[i];
				spring.entityB = state.balls[i + rowWidth - 1];
				spring.stiffness = stiffness;
				spring.restDistance = 1.4142f;
			}
		}

		// Fix corners
		registry.getComponent<RigidBody2D>(state.balls[0]).isFixed = true;
		registry.setEntityDirty<RigidBody2D>(state.balls[0], true);
		if (numBalls > rowWidth)
		{
			registry.getComponent<RigidBody2D>(state.balls[rowWidth - 1]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(state.balls[rowWidth - 1], true);
			registry.getComponent<RigidBody2D>(state.balls[rowWidth]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(state.balls[rowWidth], true);
			registry.getComponent<RigidBody2D>(state.balls[(2 * rowWidth) - 1]).isFixed = true;
			registry.setEntityDirty<RigidBody2D>(state.balls[(2 * rowWidth) - 1], true);
		}
	}

	void setupShapesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto groundMat = services.materials2D().createMaterial("ground");

		// Add base shapes (walls, ground, custom)
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 1.0f},
												  {Primitives::SineWave::PERIOD, 0.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = groundMat});

		state.star = services.shapes().addShape({.shapeId = DefaultShapes::STAR,
												 .variables = {25.0f, 10.0f, 5.0f, 0.5f, 13.0f, 5.0f},
												 .material = groundMat});

		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 15.0f},
												  {Primitives::Box::POS_Y, -98.0f},
												  {Primitives::Box::SIZE_X, 15.0f},
												  {Primitives::Box::SIZE_Y, 100.0f}},
									.material = groundMat,
									.combination = CombinationType::Addition});
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void starAnimationSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		// Animate star shape over time
		if (state.star != INVALID_ENTITY && registry.isEntityValid(state.star) &&
			registry.hasComponent<Shape>(state.star))
		{
			state.animTime += delta;
			auto& cs = registry.getComponent<Shape>(state.star);
			cs.parameters[4] = static_cast<float>((static_cast<int>(std::floor(state.animTime)) % 5) + 2);
			cs.parameters[3] = std::sin(3.1416f * state.animTime);
			registry.setComponentDirty(cs);
		}
	}

	void spawnerSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (services.input().getKey(Input::E) || services.input().getGamepadButton(Input::GamepadButton::West))
		{
			throwBalls(registry, services, state);
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
	}

	void clampToggleSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (services.input().getKeyDown(Input::R) || services.input().getGamepadButtonDown(Input::GamepadButton::South))
		{
			state.clampBalls = !state.clampBalls;
			services.physics().forEachUserData(
				[&](SimulationID, BodyUserData& data)
				{
					if (data.type == BallData::TYPE)
					{
						auto& ballData = static_cast<BallData&>(data);
						ballData.shouldClamp = state.clampBalls;
					}
				});
		}
	}

	void ropeCreationSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Exact original rope creation
		if (services.input().getMouseButtonDown(Input::LeftClick))
		{
			auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			Entity startEntity = registry.createEntity();
			if (startEntity != INVALID_ENTITY)
			{
				auto& startTransform = registry.addComponent<Transform>(startEntity);
				startTransform.position = vec3(world.x, world.y, 0.0f);
				auto& rb = registry.addComponent<RigidBody2D>(startEntity);
				rb.isFixed = true;
				auto& startSdf = registry.addComponent<Dot>(startEntity);
				startSdf.materialId = 5;

				state.creatingRope = true;
				state.startRopeBall = startEntity;
				state.lastSelectedBall = startEntity;
				state.currentRopeBalls.clear();
				state.currentRopeBalls.push_back(startEntity);
				state.currentRopeSprings.clear();
				state.startRopeMousePos = world;
			}
		}

		if (state.creatingRope)
		{
			auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
			vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

			const bool a = distance(state.startRopeMousePos, world) > state.currentRopeBalls.size() * 1.0f;
			const bool b = state.currentRopeBalls.size() == 1 && world != state.startRopeMousePos;
			if (a || b)
			{
				Entity newBall = registry.createEntity();
				if (newBall != INVALID_ENTITY)
				{
					auto& newTransform = registry.addComponent<Transform>(newBall);
					newTransform.position = vec3(world.x, world.y, 0.0f);
					auto& rb = registry.addComponent<RigidBody2D>(newBall);
					rb.isFixed = true;
					auto& newSdf = registry.addComponent<Dot>(newBall);
					newSdf.materialId = 5;

					// Connect to last selected ball with a spring
					Entity springEnt = registry.createEntity();
					if (springEnt != INVALID_ENTITY)
					{
						auto& spring = registry.addComponent<WeirdEngine::Spring>(springEnt);
						spring.entityA = state.lastSelectedBall;
						spring.entityB = newBall;
						spring.stiffness = 1.0f;
						spring.restDistance = 1.0f;

						// Unfix the last selected ball if it's not the start ball
						if (state.lastSelectedBall != state.startRopeBall)
						{
							auto& lastRb = registry.getComponent<RigidBody2D>(state.lastSelectedBall);
							lastRb.isFixed = false;
							registry.setComponentDirty(lastRb);
						}

						state.lastSelectedBall = newBall;
						state.currentRopeBalls.push_back(newBall);
						state.currentRopeSprings.push_back(springEnt);
					}
					else
					{
						registry.destroyEntity(newBall);
					}
				}
			}

			if (state.lastSelectedBall != INVALID_ENTITY && registry.isEntityValid(state.lastSelectedBall) &&
				registry.hasComponent<Transform>(state.lastSelectedBall))
			{
				auto& currentTransform = registry.getComponent<Transform>(state.lastSelectedBall);
				currentTransform.position = vec3(world.x, world.y, 0.0f);
				registry.setComponentDirty(currentTransform);
			}
		}

		if (services.input().getMouseButtonUp(Input::LeftClick))
		{
			state.creatingRope = false;
			state.startRopeBall = INVALID_ENTITY;
			state.lastSelectedBall = INVALID_ENTITY;
			state.currentRopeBalls.clear();
			state.currentRopeSprings.clear();
		}
	}

	void ballDragSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (services.input().getMouseButtonDown(Input::RightClick))
		{
			if (state.creatingRope)
			{
				for (auto ball : state.currentRopeBalls)
				{
					if (ball == state.draggedBall)
					{
						state.draggedBall = INVALID_ENTITY;
					}
					state.toDeleteRopeBalls.push(ball);
				}

				for (auto spring : state.currentRopeSprings)
				{
					state.toDeleteRopeSprings.push(spring);
				}

				state.creatingRope = false;
				state.startRopeBall = INVALID_ENTITY;
				state.lastSelectedBall = INVALID_ENTITY;
				state.currentRopeBalls.clear();
				state.currentRopeSprings.clear();
			}
			else
			{
				// Find rigidbody where user right clicked and start dragging it
				auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
				vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

				Entity hitEntity = services.physics().getRigidbodyAt(world);
				if (hitEntity != INVALID_ENTITY && registry.hasComponent<RigidBody2D>(hitEntity))
				{
					state.draggedBall = hitEntity;
					auto& rb = registry.getComponent<RigidBody2D>(state.draggedBall);
					rb.isFixed = true;
					registry.setComponentDirty(rb);
				}
			}
		}

		if (state.draggedBall != INVALID_ENTITY)
		{
			if (!registry.isEntityValid(state.draggedBall) || !registry.hasComponent<RigidBody2D>(state.draggedBall) ||
				!registry.hasComponent<Transform>(state.draggedBall))
			{
				state.draggedBall = INVALID_ENTITY;
			}
			else if (services.input().getMouseButton(Input::RightClick))
			{
				auto& cam = registry.getComponent<Transform>(services.render().getCameraEntity());
				vec2 screen = {services.input().getMouseX(), services.input().getMouseY()};
				vec2 world = ECS::Camera::screenPositionToWorldPosition2D(cam, screen);

				auto& currentTransform = registry.getComponent<Transform>(state.draggedBall);
				currentTransform.position = vec3(world.x, world.y, 0.0f);
				registry.setComponentDirty(currentTransform);

				auto& rb = registry.getComponent<RigidBody2D>(state.draggedBall);
				rb.velocity = vec2(0.0f);
				registry.setComponentDirty(rb);
			}
			else if (services.input().getMouseButtonUp(Input::RightClick))
			{
				auto& rb = registry.getComponent<RigidBody2D>(state.draggedBall);
				rb.isFixed = false;
				registry.setComponentDirty(rb);
				state.draggedBall = INVALID_ENTITY;
			}
		}
	}

	void ropeCleanupSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if ((services.time().time() - state.deleteRopeBallsTimer) >= 0.02f &&
			(!state.toDeleteRopeBalls.empty() || !state.toDeleteRopeSprings.empty()))
		{
			state.deleteRopeBallsTimer = services.time().time();

			if (!state.toDeleteRopeSprings.empty())
			{
				Entity springToDelete = state.toDeleteRopeSprings.top();
				state.toDeleteRopeSprings.pop();

				registry.destroyEntity(springToDelete);
			}

			if (!state.toDeleteRopeBalls.empty())
			{
				Entity ballToDelete = state.toDeleteRopeBalls.top();
				state.toDeleteRopeBalls.pop();

				if (ballToDelete == state.draggedBall)
				{
					state.draggedBall = INVALID_ENTITY;
				}
				registry.destroyEntity(ballToDelete);
			}
		}
	}

	void fallCleanupSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Despawn non-fixed balls that have fallen deep below the scene
		std::vector<Entity> fallenBalls;
		registry.forEach<RigidBody2D, Transform>(
			[&](Entity e, RigidBody2D& rb, Transform& t)
			{
				if (!rb.isFixed && t.position.y < -200.0f)
				{
					fallenBalls.push_back(e);
				}
			});
		for (Entity e : fallenBalls)
		{
			if (e == state.draggedBall)
			{
				state.draggedBall = INVALID_ENTITY;
			}
			registry.destroyEntity(e);
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
} // namespace RopeSceneNamespace

void RopeScene::onPhysicsStep(Simulation2D& simulation)
{
	using namespace RopeSceneNamespace;
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