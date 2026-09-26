#include "AquariumScene.h"

#include "globals.h"
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

	void createJellyfish(Registry& registry, ServiceProvider& services, float x, float y, Material2DHandle material,
						 float scale, float phase)
	{
		Entity bellEntity = registry.createEntity();
		auto& t = registry.addComponent<Transform>(bellEntity);
		t.position = vec3(x, y, 0.0f);
		auto& dot = registry.addComponent<Dot>(bellEntity);
		dot.materialId = material.id;
		registry.addComponent<RigidBody2D>(bellEntity);

		Entity bellShape = services.shapes().addShape({.shapeId = DefaultShapes::STAR,
													   .variables = {x, y, 2.5f * scale, 0.8f, 6.0f, 2.0f},
													   .material = material,
													   .combination = CombinationType::Addition,
													   .hasCollision = false});

		auto& jf = registry.addComponent<JellyfishComponent>(bellEntity);
		jf.bellShape = bellShape;
		jf.pulsePhase = phase;
		jf.pulseSpeed = 0.5f + static_cast<float>(std::rand() % 100) * 0.01f;

		float initialAngle = static_cast<float>(std::rand() % 628) * 0.01f;
		jf.direction = vec2(std::cos(initialAngle), std::sin(initialAngle));
		jf.directionChanged = false;

		constexpr int numTentacles = 6;
		constexpr int segmentsPerTentacle = 10;

		for (int t_idx = 0; t_idx < numTentacles; t_idx++)
		{
			float angle = (t_idx / static_cast<float>(numTentacles)) * 3.14159f - 3.14159f / 2.0f;
			float offsetX = std::cos(angle) * 1.5f * scale;

			for (int s = 0; s < segmentsPerTentacle; s++)
			{
				Entity segment = registry.createEntity();
				auto& st = registry.addComponent<Transform>(segment);
				st.position = vec3(x + offsetX, y - 2.5f * scale - s * 0.8f, 0.0f);
				auto& sd = registry.addComponent<Dot>(segment);
				sd.materialId = material.id;
				registry.addComponent<RigidBody2D>(segment);

				jf.tentacleSegments.push_back(segment);

				if (s > 0)
				{
					Entity springEnt = registry.createEntity();
					auto& spring = registry.addComponent<Spring>(springEnt);
					spring.entityA = jf.tentacleSegments[jf.tentacleSegments.size() - 2];
					spring.entityB = segment;
					spring.stiffness = 0.8f;
					spring.restDistance = 0.8f;
				}
			}

			Entity springEnt = registry.createEntity();
			auto& spring = registry.addComponent<Spring>(springEnt);
			spring.entityA = bellEntity;
			spring.entityB = jf.tentacleSegments[t_idx * segmentsPerTentacle];
			spring.stiffness = 0.8f;
			spring.restDistance = 0.8f;
		}
	}

	void createEel(Registry& registry, ServiceProvider& services, float startX, float startY, int segmentCount,
				   float segmentSpacing, Material2DHandle material)
	{
		Entity head = registry.createEntity();
		auto& ht = registry.addComponent<Transform>(head);
		ht.position = vec3(startX, startY, 0.0f);
		auto& hd = registry.addComponent<Dot>(head);
		hd.materialId = material.id;
		auto& hrb = registry.addComponent<RigidBody2D>(head);
		hrb.mass = 2.0f;

		auto& eel = registry.addComponent<EelComponent>(head);
		eel.segments.push_back(head);
		eel.speed = 0.8f;
		eel.phaseOffset = static_cast<float>(std::rand() % 628) * 0.01f;
		eel.segmentSpacing = segmentSpacing;
		eel.baseMaterial = material.id;

		float angle = static_cast<float>(std::rand() % 628) * 0.01f;
		eel.direction = vec2(std::cos(angle), std::sin(angle));

		float spacing = segmentSpacing;

		for (int i = 1; i < segmentCount; i++)
		{
			Entity segment = registry.createEntity();
			auto& st = registry.addComponent<Transform>(segment);
			st.position = vec3(startX - i * spacing, startY, 0.0f);
			auto& sd = registry.addComponent<Dot>(segment);
			sd.materialId = material.id;
			auto& srb = registry.addComponent<RigidBody2D>(segment);
			srb.mass = 1.0f - (i / static_cast<float>(segmentCount)) * 0.5f;

			eel.segments.push_back(segment);

			Entity springEnt = registry.createEntity();
			auto& spring = registry.addComponent<Spring>(springEnt);
			spring.entityA = eel.segments[i - 1];
			spring.entityB = segment;
			spring.stiffness = 0.35f;
			spring.restDistance = spacing;
		}
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

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = vec3(TANK_CX, TANK_CY, 45.0f);
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

		auto& jellyMat0 = services.materials2D().createMaterial("jelly_red");
		jellyMat0.color = vec4(ColorPalette::Red, 0.9f);

		auto& jellyMat1 = services.materials2D().createMaterial("jelly_yellow");
		jellyMat1.color = vec4(ColorPalette::Yellow, 0.9f);

		auto& jellyMat2 = services.materials2D().createMaterial("jelly_cyan");
		jellyMat2.color = vec4(ColorPalette::Cyan, 0.9f);

		auto& jellyMat3 = services.materials2D().createMaterial("jelly_pink");
		jellyMat3.color = vec4(ColorPalette::Pink, 0.9f);

		auto& eelMat0 = services.materials2D().createMaterial("eel_red");
		eelMat0.color = ColorPalette::Red;

		auto& eelMat1 = services.materials2D().createMaterial("eel_orange");
		eelMat1.color = ColorPalette::Orange;

		auto& eelMat2 = services.materials2D().createMaterial("eel_blue");
		eelMat2.color = ColorPalette::Blue;

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
			Entity seaweed = services.shapes().addShape({.shapeId = DefaultShapes::SINE,
														 .variables = {{Primitives::SineWave::AMPLITUDE, 3.0f},
																	   {Primitives::SineWave::PERIOD, 1.2f},
																	   {Primitives::SineWave::SPEED, 2.5f}},
														 .material = seaweedDark});
			auto& sw = registry.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 0.0f;
		}

		{
			Entity seaweed = services.shapes().addShape({.shapeId = DefaultShapes::SINE,
														 .variables = {{Primitives::SineWave::AMPLITUDE, 2.5f},
																	   {Primitives::SineWave::PERIOD, 1.5f},
																	   {Primitives::SineWave::SPEED, 2.0f}},
														 .material = seaweedLight});
			auto& sw = registry.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 1.0f;
		}

		// Tank boundary shapes
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, TANK_CX},
												  {Primitives::Box::POS_Y, TANK_CY},
												  {Primitives::Box::SIZE_X, TANK_W},
												  {Primitives::Box::SIZE_Y, TANK_H}},
									.material = tankMat});

		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, TANK_CX},
												  {Primitives::Box::POS_Y, TANK_CY},
												  {Primitives::Box::SIZE_X, TANK_W - 4.0f},
												  {Primitives::Box::SIZE_Y, TANK_H - 4.0f}},
									.material = tankMat,
									.combination = CombinationType::Subtraction});

		// Floor sand
		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, TANK_CX},
												  {Primitives::Box::POS_Y, TANK_BOTTOM + 2.0f},
												  {Primitives::Box::SIZE_X, TANK_W},
												  {Primitives::Box::SIZE_Y, 4.0f}},
									.material = seaweedDark,
									.combination = CombinationType::Addition});
	}

	void spawnCreaturesSystem(Registry& registry, ServiceProvider& services)
	{
		createJellyfish(registry, services, 0.0f, 25.0f, services.materials2D().get("jelly_red").id, 1.3f, 0.0f);
		createJellyfish(registry, services, 15.0f, 20.0f, services.materials2D().get("jelly_yellow").id, 1.6f, 1.5f);
		createJellyfish(registry, services, 30.0f, 28.0f, services.materials2D().get("jelly_cyan").id, 1.1f, 3.0f);
		createJellyfish(registry, services, 40.0f, 15.0f, services.materials2D().get("jelly_pink").id, 1.4f, 4.5f);

		createEel(registry, services, -10.0f, 50.0f, 20, 1.0f, services.materials2D().get("eel_red").id);
		createEel(registry, services, 40.0f, 40.0f, 18, 1.0f, services.materials2D().get("eel_orange").id);
		createEel(registry, services, 10.0f, 30.0f, 22, 0.9f, services.materials2D().get("eel_blue").id);

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

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
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

	void jellyfishSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(
			cameraTransform, vec2(services.input().getMouseX(), services.input().getMouseY()));

		registry.forEach<JellyfishComponent, Transform, RigidBody2D>(
			[&](Entity, JellyfishComponent& jf, Transform& bellT, RigidBody2D& rb)
			{
				auto& cs = registry.getComponent<Shape>(jf.bellShape);
				cs.parameters[0] = bellT.position.x;
				cs.parameters[1] = bellT.position.y;
				float pulse = std::sin(state.time * jf.pulseSpeed + jf.pulsePhase);
				cs.parameters[3] = 1.0f + pulse * 0.8f;
				cs.parameters[2] = 2.5f + pulse * 0.3f;
				registry.setComponentDirty(cs);

				float pulseUp = (pulse > 0.0f) ? pulse * pulse * 20.5f : 0.0f;
				rb.pendingImpulseForce += jf.direction * pulseUp;
				rb.pendingImpulseForce += vec2(0.0f, 8.0f);

				if (pulse < 0.0f && !jf.directionChanged)
				{
					constexpr float MAX_ANGLE = 45.0f;
					float angle =
						static_cast<float>((std::rand() % 100) - 50) * 0.01f * MAX_ANGLE * 3.14159265f / 180.0f;
					float cosA = cos(angle);
					float sinA = sin(angle);
					jf.direction = vec2(cosA * jf.direction.x - sinA * jf.direction.y,
										sinA * jf.direction.x + cosA * jf.direction.y);
					jf.direction = normalize(jf.direction);
					jf.directionChanged = true;
				}
				else if (pulse >= 0.0f)
				{
					jf.directionChanged = false;
				}

				if (bellT.position.x < TANK_LEFT + 5.0f)
					rb.pendingImpulseForce += vec2(1.5f, 0.0f);
				if (bellT.position.x > TANK_RIGHT - 5.0f)
					rb.pendingImpulseForce += vec2(-1.5f, 0.0f);
				if (bellT.position.y < TANK_BOTTOM + 5.0f)
					rb.pendingImpulseForce += vec2(0.0f, 2.0f);
				if (bellT.position.y > TANK_TOP - 5.0f)
					rb.pendingImpulseForce += vec2(0.0f, -1.5f);

				vec2 toMouse = mouseWorld - vec2(bellT.position);
				float dist = length(toMouse);
				if (dist < 20.0f && dist > 0.1f)
				{
					float strength = (1.0f - dist / 20.0f) * 0.8f;
					rb.pendingImpulseForce += normalize(toMouse) * strength;
				}
			});
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

	void eelSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		registry.forEach<EelComponent>(
			[&](Entity, EelComponent& eel)
			{
				if (std::rand() % 120 == 0)
				{
					float angle = static_cast<float>((std::rand() % 30) - 15) * 3.14159265f / 180.0f;
					float cosA = cos(angle);
					float sinA = sin(angle);
					eel.direction = vec2(cosA * eel.direction.x - sinA * eel.direction.y,
										 sinA * eel.direction.x + cosA * eel.direction.y);
					eel.direction = normalize(eel.direction);
				}

				Entity head = eel.segments[0];
				auto& headT = registry.getComponent<Transform>(head);
				vec2 headPos(headT.position);

				auto& headRb = registry.getComponent<RigidBody2D>(head);
				headRb.pendingContinuousForce += eel.direction * eel.speed * 120.0f;

				{
					auto foodArray = registry.getComponentArray<FishFood>();
					auto transformArray = registry.getComponentArray<Transform>();
					constexpr float eatRadius = 2.0f;
					for (size_t fi = 0; fi < foodArray->getSize(); fi++)
					{
						auto& ff = foodArray->getDataAtIdx(fi);
						if (ff.eaten)
							continue;
						Entity foodEntity = foodArray->getEntityAtIdx(fi);
						if (!transformArray->hasData(foodEntity))
							continue;
						auto& foodT = transformArray->getDataFromEntity(foodEntity);
						vec2 toFood = vec2(foodT.position) - headPos;
						if (toFood.x * toFood.x + toFood.y * toFood.y < eatRadius * eatRadius)
						{
							ff.eaten = true;
							registry.destroyEntity(foodEntity);

							Entity lastSeg = eel.segments.back();
							Entity prevSeg =
								eel.segments.size() > 1 ? eel.segments[eel.segments.size() - 2] : eel.segments[0];
							auto& lastT = transformArray->getDataFromEntity(lastSeg);
							auto& prevT = transformArray->getDataFromEntity(prevSeg);

							vec2 tailDir = vec2(lastT.position) - vec2(prevT.position);
							float tailLen = length(tailDir);
							if (tailLen < 0.001f)
								tailDir = vec2(0.0f, -1.0f);
							else
								tailDir /= tailLen;

							Entity newSeg = registry.createEntity();
							auto& nt = registry.addComponent<Transform>(newSeg);
							nt.position = vec3(lastT.position.x + tailDir.x * eel.segmentSpacing,
											   lastT.position.y + tailDir.y * eel.segmentSpacing, 0.0f);

							auto& nd = registry.addComponent<Dot>(newSeg);
							nd.materialId = static_cast<uint16_t>(eel.baseMaterial);

							registry.addComponent<RigidBody2D>(newSeg);

							Entity springEnt = registry.createEntity();
							auto& spring = registry.addComponent<Spring>(springEnt);
							spring.entityA = lastSeg;
							spring.entityB = newSeg;
							spring.stiffness = 0.25f;
							spring.restDistance = eel.segmentSpacing;

							eel.segments.push_back(newSeg);
							break;
						}
					}
				}

				for (size_t i = 1; i < eel.segments.size(); i++)
				{
					auto& segT = registry.getComponent<Transform>(eel.segments[i]);
					auto& prevT = registry.getComponent<Transform>(eel.segments[i - 1]);
					vec2 bodyDir = vec2(segT.position) - vec2(prevT.position);
					float bodyLen = length(bodyDir);

					if (bodyLen > 0.001f)
					{
						vec2 normal(-bodyDir.y / bodyLen, bodyDir.x / bodyLen);
						float wave = std::sin(state.time * 4.0f - i * 0.4f + eel.phaseOffset);
						auto& segRb = registry.getComponent<RigidBody2D>(eel.segments[i]);
						segRb.pendingContinuousForce += normal * wave * 15.0f;
					}
				}
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

		auto eelArray = registry.getComponentArray<EelComponent>();
		for (size_t i = 0; i < eelArray->getSize(); i++)
		{
			auto& eel = eelArray->getDataAtIdx(i);
			if (eel.segments[0] == event.entity)
			{
				eel.direction = vec2(-eel.direction.y, eel.direction.x);
				break;
			}
		}

		if (registry.hasComponent<JellyfishComponent>(event.entity))
		{
			auto& jf = registry.getComponent<JellyfishComponent>(event.entity);
			jf.direction = -jf.direction;
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

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace AquariumSceneNamespace