#include "MouseCollisionScene.h"

#include <cmath>

using namespace WeirdEngine;

namespace MouseCollisionSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "MouseCollisionScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& baseMat = services.materials2D().createMaterial("dot_base");
		baseMat.color = ColorPalette::Black;
		state.baseDotMat = baseMat.id;

		auto& wallMat = services.materials2D().createMaterial("wall");
		wallMat.color = ColorPalette::LightGray;

		auto& cursorMat = services.materials2D().createMaterial("cursor");
		cursorMat.color = vec4(ColorPalette::Yellow, 0.5f);

		for (int i = 0; i < 10; ++i)
		{
			auto& mat = services.materials2D().createMaterial("hit_" + std::to_string(i));
			mat.color = ColorPalette::Default[(6 + i) % ColorPalette::Default.size()];
			state.hitMats.push_back(mat.id);
		}
	}

	void setupBoundariesSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		auto& wallMat = services.materials2D().createMaterial("wall");
		auto& cursorMat = services.materials2D().createMaterial("cursor");

		// Floor
		services.shapes().addShape({.shapeId = DefaultShapes::SineWave,
									.variables = {{DefaultShapes::SineWave::Amplitude, 0.0f},
												  {DefaultShapes::SineWave::Period, 1.5f},
												  {DefaultShapes::SineWave::Speed, 1.0f}},
									.material = wallMat});

		// Wall right
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, 35.0f},
												  {DefaultShapes::Box::PosY, 0.0f},
												  {DefaultShapes::Box::SizeX, 5.0f},
												  {DefaultShapes::Box::SizeY, 30.0f}},
									.material = wallMat});

		// Wall left
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, -5.0f},
												  {DefaultShapes::Box::PosY, 0.0f},
												  {DefaultShapes::Box::SizeX, 5.0f},
												  {DefaultShapes::Box::SizeY, 30.0f}},
									.material = wallMat});

		state.cursorShape = services.shapes().addShape({.shapeId = DefaultShapes::Circle,
														.variables = {{DefaultShapes::Circle::PosX, -15.0f},
																	  {DefaultShapes::Circle::PosY, 50.0f},
																	  {DefaultShapes::Circle::Radius, 5.0f}},
														.material = cursorMat});
	}

	void spawnDotsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		for (size_t i = 0; i < 9900; i++)
		{
			float y = static_cast<float>(i / 20);
			float x = 5.0f + static_cast<float>(i % 20) + std::sin(y);

			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, 0.0f);

			Dot& dot = registry.addComponent<Dot>(entity);
			dot.materialId = state.baseDotMat.id;

			registry.addComponent<RigidBody2D>(entity);
			registry.addComponent<CollisionCounter>(entity);
		}
	}

	void cursorTrackingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		if (registry.isEntityValid(state.cursorShape) && registry.hasComponent<Shape>(state.cursorShape))
		{
			Shape& cs = registry.getComponent<Shape>(state.cursorShape);
			auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
			float x = services.input().getMouseX();
			float y = services.input().getMouseY();

			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.parameters[0] = mousePositionInWorld.x;
			cs.parameters[1] = mousePositionInWorld.y;
			registry.setComponentDirty(cs);
		}
	}

	void ballEntityCollisionSystem(Registry& registry, ServiceProvider& services, EntityCollisionEvent& event)
	{
		State& state = getState(registry);
		constexpr int COLLISIONS_PER_MATERIAL = 50;

		auto updateEntity = [&](Entity ent)
		{
			if (registry.isEntityValid(ent) && registry.hasComponent<CollisionCounter>(ent))
			{
				auto& counter = registry.getComponent<CollisionCounter>(ent);
				counter.count++;

				if (counter.count <= 10 * COLLISIONS_PER_MATERIAL && counter.count % COLLISIONS_PER_MATERIAL == 0)
				{
					auto& dot = registry.getComponent<Dot>(ent);
					int stage = (counter.count / COLLISIONS_PER_MATERIAL) - 1;
					if (stage >= 0 && stage < static_cast<int>(state.hitMats.size()))
					{
						dot.materialId = state.hitMats[stage].id;
						registry.setComponentDirty(dot);
					}

					if (counter.count == 10 * COLLISIONS_PER_MATERIAL)
					{
						dot.materialId = state.baseDotMat.id;
						registry.setComponentDirty(dot);
					}
				}
			}
		};

		updateEntity(event.entityA);
		updateEntity(event.entityB);
	}

	void ballShapeCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		State& state = getState(registry);
		if (registry.isEntityValid(event.entity) && registry.hasComponent<CollisionCounter>(event.entity))
		{
			auto& counter = registry.getComponent<CollisionCounter>(event.entity);
			counter.count = 0;
			auto& dot = registry.getComponent<Dot>(event.entity);
			dot.materialId = state.baseDotMat.id;
			registry.setComponentDirty(dot);
		}
	}

} // namespace MouseCollisionSceneNamespace