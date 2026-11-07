#include "DestroyScene.h"

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

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(0.0f, 0.0f, 100.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		for (int i = 0; i < 8; ++i)
		{
			auto& mat = services.materials2D().createMaterial("mat_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			state.materials[i] = mat.id;
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
						auto dotArray = registry.getComponentArray<Dot>();
						if (!dotArray || dotArray->getSize() < 1000)
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
							}
						}
						break;
					}
					case 1:
					{
						auto shapeArray = registry.getComponentArray<Shape>();
						if (!shapeArray || shapeArray->getSize() < 20)
						{
							float x = static_cast<float>(std::rand() % 200) - 100.0f;
							float y = static_cast<float>(std::rand() % 100) - 50.0f;
							float w = static_cast<float>(std::rand() % 4 + 1);
							float h = static_cast<float>(std::rand() % 4 + 1);
							auto material = state.materials[std::rand() % state.materials.size()];
							services.shapes().addShape({.shapeId = DefaultShapes::Box,
														.variables = {{DefaultShapes::Box::PosX, x},
																	  {DefaultShapes::Box::PosY, y},
																	  {DefaultShapes::Box::SizeX, w},
																	  {DefaultShapes::Box::SizeY, h}},
														.material = material,
														.combination = CombinationType::Addition});
						}
						break;
					}
					case 2:
					{
						auto dotArray = registry.getComponentArray<Dot>();
						auto distArray = registry.getComponentArray<DistanceConstraint>();
						auto springArray = registry.getComponentArray<Spring>();
						size_t totalConstraints =
							(distArray ? distArray->getSize() : 0) + (springArray ? springArray->getSize() : 0);

						if (dotArray && dotArray->getSize() >= 2 && totalConstraints < 50)
						{
							int idx1 = std::rand() % dotArray->getSize();
							int idx2 = std::rand() % dotArray->getSize();
							if (idx1 != idx2)
							{
								Entity constraintEnt = registry.createEntity();
								if (std::rand() % 2 == 0)
								{
									auto& constraint =
										registry.addComponent<WeirdEngine::DistanceConstraint>(constraintEnt);
									constraint.entityA = dotArray->getEntityAtIdx(idx1);
									constraint.entityB = dotArray->getEntityAtIdx(idx2);
									constraint.distance = 3.0f + static_cast<float>(std::rand() % 5);
								}
								else
								{
									auto& spring = registry.addComponent<WeirdEngine::Spring>(constraintEnt);
									spring.entityA = dotArray->getEntityAtIdx(idx1);
									spring.entityB = dotArray->getEntityAtIdx(idx2);
									spring.restDistance = 3.0f + static_cast<float>(std::rand() % 5);
									spring.stiffness = 0.5f;
								}
							}
						}
						break;
					}
					case 3:
					{
						auto dotArray = registry.getComponentArray<Dot>();
						if (dotArray && dotArray->getSize() > 0)
						{
							int toRemove = (std::min)(10, static_cast<int>(dotArray->getSize()));
							for (int j = 0; j < toRemove && dotArray->getSize() > 0; ++j)
							{
								int idx = std::rand() % dotArray->getSize();
								Entity e = dotArray->getEntityAtIdx(idx);

								registry.forEach<DistanceConstraint>(
									[&](Entity cEnt, DistanceConstraint& c)
									{
										if (c.entityA == e || c.entityB == e)
											registry.destroyEntity(cEnt);
									});

								registry.forEach<Spring>(
									[&](Entity sEnt, Spring& s)
									{
										if (s.entityA == e || s.entityB == e)
											registry.destroyEntity(sEnt);
									});

								registry.destroyEntity(e);
							}
						}
						break;
					}
					case 4:
					{
						auto shapeArray = registry.getComponentArray<Shape>();
						if (shapeArray && shapeArray->getSize() > 0)
						{
							int idx = std::rand() % shapeArray->getSize();
							registry.destroyEntity(shapeArray->getEntityAtIdx(idx));
						}
						break;
					}
					case 5:
					{
						auto distArray = registry.getComponentArray<DistanceConstraint>();
						auto springArray = registry.getComponentArray<Spring>();
						size_t distCount = distArray ? distArray->getSize() : 0;
						size_t springCount = springArray ? springArray->getSize() : 0;
						if (distCount + springCount > 0)
						{
							size_t pick = std::rand() % (distCount + springCount);
							if (pick < distCount)
							{
								registry.destroyEntity(distArray->getEntityAtIdx(pick));
							}
							else
							{
								registry.destroyEntity(springArray->getEntityAtIdx(pick - distCount));
							}
						}
						break;
					}
				}
			}
		}
	}
} // namespace DestroySceneNamespace
