#include "DestroyScene.h"

#include "globals.h"
#include <algorithm>
#include <cstdlib>

using namespace WeirdEngine;

namespace DestroySceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "DestroyScene State is missing: stateInitSystem must run first");
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

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		for (int i = 0; i < 8; ++i)
		{
			auto& mat = services.materials2D().createMaterial("mat_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			state.materials.push_back(mat.id);
		}
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void stressTestSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		state.timer += delta;
		if (state.timer > 0.02f)
		{
			state.timer = 0.0f;

			for (int i = 0; i < 5; ++i)
			{
				int action = std::rand() % 6;

				switch (action)
				{
					case 0:
					{
						if (state.testBalls.size() < 1000)
						{
							for (int j = 0; j < 10; ++j)
							{
								Entity e = registry.createEntity();
								auto& t = registry.addComponent<Transform>(e);
								t.position = vec3(static_cast<float>(std::rand() % 200) - 100.0f,
												  static_cast<float>(std::rand() % 100) - 50.0f, 0.0f);
								registry.setComponentDirty(t);
								auto& ui = registry.addComponent<Dot>(e);
								ui.materialId = state.materials[e % state.materials.size()].id;
								registry.addComponent<RigidBody2D>(e);
								state.testBalls.push_back(e);
							}
						}
						break;
					}
					case 1:
					{
						if (state.testShapes.size() < 20)
						{
							float x = static_cast<float>(std::rand() % 200) - 100.0f;
							float y = static_cast<float>(std::rand() % 100) - 50.0f;
							float w = static_cast<float>(std::rand() % 4 + 1);
							float h = static_cast<float>(std::rand() % 4 + 1);
							auto material = state.materials[std::rand() % state.materials.size()];
							Entity shape = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
																	   .variables = {{Primitives::Box::POS_X, x},
																					 {Primitives::Box::POS_Y, y},
																					 {Primitives::Box::SIZE_X, w},
																					 {Primitives::Box::SIZE_Y, h}},
																	   .material = material,
																	   .combination = CombinationType::Addition});
							state.testShapes.push_back(shape);
						}
						break;
					}
					case 2:
					{
						if (state.testBalls.size() >= 2 && state.testConstraints.size() < 50)
						{
							int idx1 = std::rand() % state.testBalls.size();
							int idx2 = std::rand() % state.testBalls.size();
							if (idx1 != idx2)
							{
								Entity constraintEnt = registry.createEntity();
								if (std::rand() % 2 == 0)
								{
									auto& constraint =
										registry.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
									constraint.entityA = state.testBalls[idx1];
									constraint.entityB = state.testBalls[idx2];
									constraint.distance = 3.0f + static_cast<float>(std::rand() % 5);
								}
								else
								{
									auto& spring = registry.addComponent<WeirdEngine::Spring>(constraintEnt);
									spring.entityA = state.testBalls[idx1];
									spring.entityB = state.testBalls[idx2];
									spring.restDistance = 3.0f + static_cast<float>(std::rand() % 5);
									spring.stiffness = 0.5f;
								}
								state.testConstraints.push_back(constraintEnt);
							}
						}
						break;
					}
					case 3:
					{
						if (!state.testBalls.empty())
						{
							int toRemove = (std::min)(10, static_cast<int>(state.testBalls.size()));
							for (int j = 0; j < toRemove; ++j)
							{
								int idx = std::rand() % state.testBalls.size();
								Entity e = state.testBalls[idx];

								for (auto it = state.testConstraints.begin(); it != state.testConstraints.end();)
								{
									bool shouldRemove = false;
									if (registry.hasComponent<WeirdEngine::DistanceConstraint>(*it))
									{
										auto& c = registry.getComponent<WeirdEngine::DistanceConstraint>(*it);
										if (c.entityA == e || c.entityB == e)
											shouldRemove = true;
									}
									else if (registry.hasComponent<WeirdEngine::Spring>(*it))
									{
										auto& s = registry.getComponent<WeirdEngine::Spring>(*it);
										if (s.entityA == e || s.entityB == e)
											shouldRemove = true;
									}

									if (shouldRemove)
									{
										registry.destroyEntity(*it);
										it = state.testConstraints.erase(it);
									}
									else
									{
										++it;
									}
								}

								registry.destroyEntity(e);
								state.testBalls.erase(state.testBalls.begin() + idx);
							}
						}
						break;
					}
					case 4:
					{
						if (!state.testShapes.empty())
						{
							int idx = std::rand() % state.testShapes.size();
							registry.destroyEntity(state.testShapes[idx]);
							state.testShapes.erase(state.testShapes.begin() + idx);
						}
						break;
					}
					case 5:
					{
						if (!state.testConstraints.empty())
						{
							int idx = std::rand() % state.testConstraints.size();
							registry.destroyEntity(state.testConstraints[idx]);
							state.testConstraints.erase(state.testConstraints.begin() + idx);
						}
						break;
					}
				}
			}
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
} // namespace DestroySceneNamespace