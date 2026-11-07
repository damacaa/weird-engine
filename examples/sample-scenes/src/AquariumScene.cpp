#include "AquariumScene.h"

#include <cmath>
#include <cstdlib>

using namespace WeirdEngine;

namespace AquariumSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "AquariumScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void spawnBubble(Registry& registry, State& state)
	{
		float bx = TANK_LEFT + 5.0f + static_cast<float>(std::rand() % 1000) * 0.001f * (TANK_W - 10.0f);
		Entity bubble = registry.createEntity();
		auto& t = registry.addComponent<Transform>(bubble);
		t.position = vec3(bx, TANK_BOTTOM + 2.0f, 0.0f);

		auto& dot = registry.addComponent<Dot>(bubble);
		dot.materialId = state.bubbleMat.id;

		auto& rb = registry.addComponent<RigidBody2D>(bubble);
		rb.mass = 0.2f;

		auto& bComp = registry.addComponent<Bubble>(bubble);
		bComp.wobbleSpeed = 3.0f + static_cast<float>(std::rand() % 100) * 0.02f;
		bComp.wobblePhase = static_cast<float>(std::rand() % 628) * 0.01f;
	}

	void spawnFood(Registry& registry, State& state, vec2 pos)
	{
		constexpr int FOOD_COUNT = 15;
		for (int i = 0; i < FOOD_COUNT; i++)
		{
			Entity food = registry.createEntity();
			auto& ft = registry.addComponent<Transform>(food);
			float ox = static_cast<float>(std::rand() % 200 - 100) * 0.04f;
			float oy = static_cast<float>(std::rand() % 200 - 100) * 0.04f;
			ft.position = vec3(pos.x + ox, pos.y + oy, 0.0f);
			registry.setComponentDirty(ft);

			auto& fd = registry.addComponent<Dot>(food);
			fd.materialId = state.foodMat.id;

			auto& frb = registry.addComponent<RigidBody2D>(food);
			frb.velocity = vec2(ox * 2.0f, oy * 2.0f);

			registry.addComponent<FishFood>(food);
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

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Sky;
		background.primaryColor = vec4(98, 129, 240, 255) / 255.0f;
		background.secondaryColor = vec4(86, 208, 197, 255) / 255.0f;
		background.scale = 0.15f;

		services.physics().setGravity(-10.0f);
		services.physics().setDamping(0.025f);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		cameraTransform.position = vec3(TANK_CX, TANK_CY, 45.0f);
		cameraTransform.rotation = vec3(0.0f, 0.0f, -1.0f);
	}

	void setupMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& seaweedDark = services.materials2D().createMaterial("seaweed_dark");
		seaweedDark.color = ColorPalette::Green;

		auto& seaweedLight = services.materials2D().createMaterial("seaweed_light");
		seaweedLight.color = ColorPalette::LightGreen;

		auto& tankMat = services.materials2D().createMaterial("tank");
		tankMat.color = vec4(ColorPalette::LightBlue, 0.5f);

		auto& foodMat = services.materials2D().createMaterial("fish_food");
		foodMat.color = ColorPalette::Orange;
		state.foodMat = foodMat.id;

		auto& bubbleMat = services.materials2D().createMaterial("aquarium_bubble");
		bubbleMat.color = vec4(ColorPalette::White, 0.55f);
		state.bubbleMat = bubbleMat.id;

		for (int i = 0; i < 6; ++i)
		{
			auto& m = services.materials2D().createMaterial("fish_" + std::to_string(i));
			m.color = ColorPalette::Default[(i * 2 + 1) % ColorPalette::Default.size()];
		}
	}

	void setupTankShapesSystem(Registry& registry, ServiceProvider& services)
	{
		auto& seaweedDark = services.materials2D().get("seaweed_dark");
		auto& seaweedLight = services.materials2D().get("seaweed_light");
		auto& tankMat = services.materials2D().get("tank");

		{
			Entity seaweed = services.shapes().addShape({.shapeId = DefaultShapes::SineWave,
														 .variables = {{DefaultShapes::SineWave::Amplitude, 3.0f},
																	   {DefaultShapes::SineWave::Period, 1.2f},
																	   {DefaultShapes::SineWave::Speed, 2.5f}},
														 .material = seaweedDark});
			auto& sw = registry.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 0.0f;
		}

		{
			Entity seaweed = services.shapes().addShape({.shapeId = DefaultShapes::SineWave,
														 .variables = {{DefaultShapes::SineWave::Amplitude, 2.5f},
																	   {DefaultShapes::SineWave::Period, 1.5f},
																	   {DefaultShapes::SineWave::Speed, 2.0f}},
														 .material = seaweedLight});
			auto& sw = registry.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 1.0f;
		}

		// Tank boundary shapes
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, TANK_CX},
												  {DefaultShapes::Box::PosY, TANK_CY},
												  {DefaultShapes::Box::SizeX, TANK_W},
												  {DefaultShapes::Box::SizeY, TANK_H}},
									.material = tankMat});

		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, TANK_CX},
												  {DefaultShapes::Box::PosY, TANK_CY},
												  {DefaultShapes::Box::SizeX, TANK_W - 4.0f},
												  {DefaultShapes::Box::SizeY, TANK_H - 4.0f}},
									.material = tankMat,
									.combination = CombinationType::Subtraction});

		// Floor sand
		services.shapes().addShape({.shapeId = DefaultShapes::Box,
									.variables = {{DefaultShapes::Box::PosX, TANK_CX},
												  {DefaultShapes::Box::PosY, TANK_BOTTOM + 2.0f},
												  {DefaultShapes::Box::SizeX, TANK_W},
												  {DefaultShapes::Box::SizeY, 4.0f}},
									.material = seaweedDark,
									.combination = CombinationType::Addition});
	}

	void spawnCreaturesSystem(Registry& registry, ServiceProvider& services)
	{
		std::vector<Material2DHandle> fishMats;
		for (int i = 0; i < 6; ++i)
		{
			fishMats.push_back(services.materials2D().get("fish_" + std::to_string(i)).id);
		}

		constexpr int NUM_FISH = 60;
		for (int i = 0; i < NUM_FISH; i++)
		{
			Entity fish = registry.createEntity();
			auto& t = registry.addComponent<Transform>(fish);
			float fx = TANK_LEFT + 5.0f + static_cast<float>(std::rand() % 1000) * 0.001f * (TANK_W - 10.0f);
			float fy = TANK_BOTTOM + 5.0f + static_cast<float>(std::rand() % 1000) * 0.001f * (TANK_H - 10.0f);
			t.position = vec3(fx, fy, 0.0f);

			auto& dot = registry.addComponent<Dot>(fish);
			dot.materialId = fishMats[i % fishMats.size()].id;

			auto& rb = registry.addComponent<RigidBody2D>(fish);
			rb.pendingImpulseForce += vec2(static_cast<float>((std::rand() % 100) - 50) * 0.05f,
										   static_cast<float>((std::rand() % 100) - 50) * 0.05f);

			auto& fishComp = registry.addComponent<Fish>(fish);
			float angle = static_cast<float>(std::rand() % 628) * 0.01f;
			fishComp.velocity = vec2(std::cos(angle), std::sin(angle)) * 3.0f;
			fishComp.maxSpeed = 5.0f;
			fishComp.separationWeight = 1.5f;
			fishComp.alignmentWeight = 1.0f;
			fishComp.cohesionWeight = 1.0f;
			fishComp.perceptionRadius = 5.0f;
		}
	}

	void feedingInputSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(
			cameraTransform, vec2(services.input().getMouseX(), services.input().getMouseY()));

		// Feed on E key or Left Click
		if (services.input().getKeyDown(Input::E) ||
			(services.input().getMouseButtonDown(Input::LeftClick) && !services.input().isUIClick()))
		{
			spawnFood(registry, state, mouseWorld);
			services.audio().playSound({0.02f, 700.0f, false, vec3(0.0f), 1});
		}
	}

	void bubbleSpawnerSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		state.bubbleTimer += delta;
		if (state.bubbleTimer > 0.35f)
		{
			state.bubbleTimer = 0.0f;
			spawnBubble(registry, state);
		}
	}

	void bubblePhysicsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();
		state.time += delta;

		std::vector<Entity> poppedBubbles;
		registry.forEach<Bubble, Transform, RigidBody2D>(
			[&](Entity e, Bubble& b, Transform& t, RigidBody2D& rb)
			{
				float wobble = std::sin(state.time * b.wobbleSpeed + b.wobblePhase) * 1.5f;
				rb.pendingContinuousForce += vec2(wobble, 15.0f);

				if (t.position.y > TANK_TOP - 4.0f)
				{
					poppedBubbles.push_back(e);
				}
			});

		for (Entity b : poppedBubbles)
		{
			if (registry.isEntityValid(b))
			{
				registry.destroyEntity(b);
			}
		}
	}

	void seaweedSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		registry.forEach<Seaweed, Shape>(
			[&](Entity, Seaweed& sw, Shape& cs)
			{
				cs.parameters[3] = std::sin(state.time * 1.5f + sw.animationOffset) * 4.0f;
				registry.setComponentDirty(cs);
			});
	}

	void fishFlockingSystem(Registry& registry, ServiceProvider& services)
	{
		float delta = services.time().deltaTime();

		std::vector<Entity> fishEntities;
		auto fishArray = registry.getComponentArray<Fish>();
		for (size_t i = 0; i < fishArray->getSize(); i++)
		{
			fishEntities.push_back(fishArray->getEntityAtIdx(i));
		}

		auto transformArray = registry.getComponentArray<Transform>();
		auto foodArray = registry.getComponentArray<FishFood>();

		for (size_t i = 0; i < fishEntities.size(); i++)
		{
			Entity e = fishEntities[i];
			if (!registry.isEntityValid(e))
				continue;

			auto& fish = registry.getComponent<Fish>(e);
			auto& t = registry.getComponent<Transform>(e);
			vec2 pos(t.position);

			if (fish.mateCooldown > 0.0f)
			{
				fish.mateCooldown -= delta;
			}

			vec2 separation(0.0f);
			vec2 alignment(0.0f);
			vec2 cohesion(0.0f);
			int neighborCount = 0;

			for (size_t j = 0; j < fishEntities.size(); j++)
			{
				if (i == j)
					continue;
				Entity otherE = fishEntities[j];
				if (!registry.isEntityValid(otherE))
					continue;

				auto& otherT = registry.getComponent<Transform>(otherE);
				vec2 otherPos(otherT.position);
				vec2 diff = pos - otherPos;
				float distSq = diff.x * diff.x + diff.y * diff.y;

				if (distSq < fish.perceptionRadius * fish.perceptionRadius && distSq > 0.0001f)
				{
					float dist = std::sqrt(distSq);
					separation += (diff / dist) / dist;
					auto& otherFish = registry.getComponent<Fish>(otherE);
					alignment += otherFish.velocity;
					cohesion += otherPos;
					neighborCount++;
				}
			}

			if (neighborCount > 0)
			{
				separation /= static_cast<float>(neighborCount);
				alignment /= static_cast<float>(neighborCount);
				alignment = normalize(alignment) * fish.maxSpeed;
				cohesion /= static_cast<float>(neighborCount);
				cohesion = normalize(cohesion - pos) * fish.maxSpeed;
			}

			vec2 boundaryForce(0.0f);
			constexpr float MARGIN = 8.0f;
			if (pos.x < TANK_LEFT + MARGIN)
				boundaryForce.x += (TANK_LEFT + MARGIN - pos.x) * 2.0f;
			if (pos.x > TANK_RIGHT - MARGIN)
				boundaryForce.x -= (pos.x - (TANK_RIGHT - MARGIN)) * 2.0f;
			if (pos.y < TANK_BOTTOM + MARGIN)
				boundaryForce.y += (TANK_BOTTOM + MARGIN - pos.y) * 2.0f;
			if (pos.y > TANK_TOP - MARGIN)
				boundaryForce.y -= (pos.y - (TANK_TOP - MARGIN)) * 2.0f;

			vec2 foodForce(0.0f);
			float nearestFoodDistSq = 999999.0f;
			Entity nearestFood = INVALID_ENTITY;

			for (size_t fi = 0; fi < foodArray->getSize(); fi++)
			{
				auto& ff = foodArray->getDataAtIdx(fi);
				if (ff.eaten)
					continue;
				Entity foodEntity = foodArray->getEntityAtIdx(fi);
				if (!transformArray->hasData(foodEntity))
					continue;
				auto& foodT = transformArray->getDataFromEntity(foodEntity);
				vec2 toFood = vec2(foodT.position) - pos;
				float fDistSq = toFood.x * toFood.x + toFood.y * toFood.y;
				if (fDistSq < 15.0f * 15.0f && fDistSq < nearestFoodDistSq)
				{
					nearestFoodDistSq = fDistSq;
					nearestFood = foodEntity;
					foodForce = normalize(toFood) * 4.0f;
				}
			}

			if (nearestFood != INVALID_ENTITY && nearestFoodDistSq < 1.5f * 1.5f)
			{
				auto& ff = registry.getComponent<FishFood>(nearestFood);
				if (!ff.eaten)
				{
					ff.eaten = true;
					registry.destroyEntity(nearestFood);
					fish.energy += 1.0f;
				}
			}

			vec2 acceleration = separation * fish.separationWeight + alignment * fish.alignmentWeight +
								cohesion * fish.cohesionWeight + boundaryForce + foodForce;

			fish.velocity += acceleration * delta;
			float speed = length(fish.velocity);
			if (speed > fish.maxSpeed)
				fish.velocity = (fish.velocity / speed) * fish.maxSpeed;
			if (speed < 1.0f)
				fish.velocity = normalize(fish.velocity) * 1.0f;

			auto& rb = registry.getComponent<RigidBody2D>(e);
			rb.pendingContinuousForce += (fish.velocity - rb.velocity) * 10.0f;
			rb.pendingContinuousForce += vec2(0.0f, 10.0f); // counter gravity
		}
	}

	void onEntityShapeCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		if (std::rand() % 8 == 0)
		{
			services.audio().playSound({0.015f, 150.0f + (std::rand() % 150), true, vec3(event.raw.position, 0.0f), 1});
		}
	}

	void onEntityCollisionSystem(Registry& registry, ServiceProvider& services, EntityCollisionEvent& event)
	{
		Entity a = event.entityA;
		Entity b = event.entityB;

		if (a == INVALID_ENTITY || b == INVALID_ENTITY)
			return;

		if (registry.hasComponent<Fish>(a) && registry.hasComponent<Fish>(b))
		{
			auto& fishA = registry.getComponent<Fish>(a);
			auto& fishB = registry.getComponent<Fish>(b);

			if (fishA.energy >= 3.0f && fishB.energy >= 3.0f && fishA.mateCooldown <= 0.0f &&
				fishB.mateCooldown <= 0.0f)
			{
				fishA.energy -= 2.0f;
				fishB.energy -= 2.0f;
				fishA.mateCooldown = 5.0f;
				fishB.mateCooldown = 5.0f;

				auto& tA = registry.getComponent<Transform>(a);
				auto& tB = registry.getComponent<Transform>(b);
				auto& dotA = registry.getComponent<Dot>(a);

				int numOffspring = 1 + (std::rand() % 4);
				for (int i = 0; i < numOffspring; i++)
				{
					Entity baby = registry.createEntity();
					auto& bt = registry.addComponent<Transform>(baby);
					float ox = static_cast<float>(std::rand() % 100 - 50) * 0.03f;
					float oy = static_cast<float>(std::rand() % 100 - 50) * 0.03f;
					bt.position = (tA.position + tB.position) * 0.5f + vec3(ox, oy, 0.0f);
					registry.setComponentDirty(bt);

					auto& bd = registry.addComponent<Dot>(baby);
					bd.materialId = static_cast<unsigned int>(dotA.materialId);

					registry.addComponent<RigidBody2D>(baby);

					auto& bFish = registry.addComponent<Fish>(baby);
					float angle = static_cast<float>(std::rand() % 628) * 0.01f;
					bFish.velocity = vec2(std::cos(angle), std::sin(angle)) * 3.0f;
					bFish.maxSpeed = fishA.maxSpeed;
					bFish.separationWeight = fishA.separationWeight;
					bFish.alignmentWeight = fishA.alignmentWeight;
					bFish.cohesionWeight = fishA.cohesionWeight;
					bFish.perceptionRadius = fishA.perceptionRadius;
				}
			}
		}
	}

} // namespace AquariumSceneNamespace