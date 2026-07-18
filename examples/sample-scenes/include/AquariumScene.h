#pragma once

#include <weird-engine.h>

#include <cmath>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "globals.h"
#include "weird-physics/components/GlobalPhysicsSettings.h"
#include "weird-physics/components/Spring.h"

using namespace WeirdEngine;

struct Fish
{
	vec2 velocity;
	float maxSpeed;
	float separationWeight;
	float alignmentWeight;
	float cohesionWeight;
	float perceptionRadius;
	float energy = 0.0f;
	float mateCooldown = 0.0f;
};

struct FishFood
{
	bool eaten = false;
};

struct JellyfishComponent
{
	Entity bellShape;
	std::vector<Entity> tentacleSegments;
	float pulsePhase;
	float pulseSpeed;
	vec2 direction;
	bool directionChanged;
};

struct Seaweed
{
	float animationOffset;
};

struct EelComponent
{
	std::vector<Entity> segments;
	float phaseOffset;
	float speed;
	vec2 direction;
	float segmentSpacing;
	int baseMaterial;
};

class AquariumScene : public Scene2D
{
public:
	AquariumScene() {}

private:
	float m_time = 0.0f;

	static constexpr float TANK_LEFT = -20.0f;
	static constexpr float TANK_RIGHT = 50.0f;
	static constexpr float TANK_BOTTOM = -10.0f;
	static constexpr float TANK_TOP = 60.0f;
	static constexpr float TANK_CX = (TANK_LEFT + TANK_RIGHT) * 0.5f;
	static constexpr float TANK_CY = (TANK_BOTTOM + TANK_TOP) * 0.5f;
	static constexpr float TANK_W = TANK_RIGHT - TANK_LEFT;
	static constexpr float TANK_H = TANK_TOP - TANK_BOTTOM;

	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		m_background.type = BackgroundType::Sky;
		m_background.primaryColor = vec4(98, 129, 240, 255) / 255.0f;
		m_background.secondaryColor = vec4(86, 208, 197, 255) / 255.0f;
		m_background.scale = 0.15f;

		Entity globalSettingsEnt = ecs.createEntity();
		auto& settings = ecs.addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = -10.0f;
		settings.damping = 0.025f;
		ecs.setComponentDirty(settings);

		createJellyfish(ecs, 0.0f, 25.0f, 4 + 0, 1.3f, 0.0f);
		createJellyfish(ecs, 15.0f, 20.0f, 4 + 3, 1.6f, 1.5f);
		createJellyfish(ecs, 30.0f, 28.0f, 4 + 6, 1.1f, 3.0f);
		createJellyfish(ecs, 40.0f, 15.0f, 4 + 9, 1.4f, 4.5f);

		createEel(ecs, -10.0f, 50.0f, 20, 1.0f, 4);
		createEel(ecs, 40.0f, 40.0f, 18, 1.0f, 8);
		createEel(ecs, 10.0f, 30.0f, 22, 0.9f, 6);

		{
			float seaweedVars[8] = {3.0f, 1.2f, 2.5f};
			Entity seaweed = addShape(DefaultShapes::SINE, seaweedVars, DisplaySettings::Green);
			auto& sw = ecs.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 0.0f;
		}

		{
			float seaweedVars[8] = {2.0f, 2.0f, 1.8f};
			Entity seaweed = addShape(DefaultShapes::SINE, seaweedVars, DisplaySettings::LightGreen);
			auto& sw = ecs.addComponent<Seaweed>(seaweed);
			sw.animationOffset = 1.5f;
		}

		{
			float boxVars[8] = {TANK_CX, TANK_CY, TANK_W, TANK_H};
			Entity box =
				addShape(DefaultShapes::BOX, boxVars, DisplaySettings::LightBlue, CombinationType::Intersection);
		}

		{
			float boxVars[8] = {TANK_CX, TANK_CY, TANK_W, TANK_H, 1.0f};
			Entity box = addShape(DefaultShapes::BOX_LINE, boxVars, DisplaySettings::LightBlue);
		}

		for (int i = 0; i < 40; i++)
		{
			Entity fish = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(fish);
			float fx = TANK_LEFT + 5.0f + static_cast<float>(std::rand() % 60);
			float fy = TANK_BOTTOM + 5.0f + static_cast<float>(std::rand() % 35);
			t.position = vec3(fx, fy, 0.0f);

			auto& dot = ecs.addComponent<Dot>(fish);
			dot.materialId = 4 + (i % 12);

			auto& rb = ecs.addComponent<RigidBody2D>(fish);
			rb.pendingImpulseForce += vec2(static_cast<float>((std::rand() % 100) - 50) * 0.05f,
										   static_cast<float>((std::rand() % 100) - 50) * 0.05f);

			auto& fishComp = ecs.addComponent<Fish>(fish);
			float angle = static_cast<float>(std::rand() % 628) * 0.01f;
			fishComp.velocity = vec2(std::cos(angle), std::sin(angle)) * 3.0f;
			fishComp.maxSpeed = 5.0f;
			fishComp.separationWeight = 1.5f;
			fishComp.alignmentWeight = 1.0f;
			fishComp.cohesionWeight = 1.0f;
			fishComp.perceptionRadius = 5.0f;
		}

		ecs.getComponent<Transform>(m_mainCamera).position = vec3(TANK_CX, TANK_CY, 45.0f);
	}

	void createJellyfish(ECSManager& ecs, float x, float y, int material, float scale, float phase)
	{
		Entity bellEntity = ecs.createEntity();
		auto& t = ecs.addComponent<Transform>(bellEntity);
		t.position = vec3(x, y, 0.0f);
		auto& dot = ecs.addComponent<Dot>(bellEntity);
		dot.materialId = material;
		auto& rb = ecs.addComponent<RigidBody2D>(bellEntity);

		float bellVars[8] = {x, y, 2.5f * scale, 0.8f, 6.0f, 2.0f};
		Entity bellShape = addShape(DefaultShapes::STAR, bellVars, material, CombinationType::Addition, false);

		auto& jf = ecs.addComponent<JellyfishComponent>(bellEntity);
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
			float angle = (t_idx / (float)numTentacles) * 3.14159f - 3.14159f / 2.0f;
			float offsetX = std::cos(angle) * 1.5f * scale;

			for (int s = 0; s < segmentsPerTentacle; s++)
			{
				Entity segment = ecs.createEntity();
				auto& st = ecs.addComponent<Transform>(segment);
				st.position = vec3(x + offsetX, y - 2.5f * scale - s * 0.8f, 0.0f);
				auto& sd = ecs.addComponent<Dot>(segment);
				sd.materialId = material;
				auto& srb = ecs.addComponent<RigidBody2D>(segment);

				jf.tentacleSegments.push_back(segment);

				if (s > 0)
				{
					Entity springEnt = ecs.createEntity();
					auto& spring = ecs.addComponent<Spring>(springEnt);
					spring.entityA = jf.tentacleSegments[jf.tentacleSegments.size() - 2];
					spring.entityB = segment;
					spring.stiffness = 0.8f;
					spring.restDistance = 0.8f;
				}
			}

			Entity springEnt = ecs.createEntity();
			auto& spring = ecs.addComponent<Spring>(springEnt);
			spring.entityA = bellEntity;
			spring.entityB = jf.tentacleSegments[t_idx * segmentsPerTentacle];
			spring.stiffness = 0.5f;
			spring.restDistance = 2.5f * scale;
		}
	}

	void createEel(ECSManager& ecs, float x, float y, int numSegments, float spacing, int baseMaterial)
	{
		float angle = static_cast<float>(std::rand() % 628) * 0.01f;
		vec2 dir(std::cos(angle), std::sin(angle));

		Entity eelEnt = ecs.createEntity();
		auto& eel = ecs.addComponent<EelComponent>(eelEnt);
		eel.phaseOffset = static_cast<float>(std::rand() % 100) * 0.0628f;
		eel.speed = 2.0f + static_cast<float>(std::rand() % 100) * 0.02f;
		eel.direction = dir;
		eel.segmentSpacing = spacing;
		eel.baseMaterial = baseMaterial;

		for (int i = 0; i < numSegments; i++)
		{
			Entity seg = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(seg);
			t.position = vec3(x + dir.x * i * spacing, y + dir.y * i * spacing, 0.0f);

			auto& dot = ecs.addComponent<Dot>(seg);
			dot.materialId = static_cast<unsigned int>(baseMaterial + (i % 4));

			ecs.addComponent<RigidBody2D>(seg);

			eel.segments.push_back(seg);

			if (i > 0)
			{
				Entity springEnt = ecs.createEntity();
				auto& spring = ecs.addComponent<Spring>(springEnt);
				spring.entityA = eel.segments[i - 1];
				spring.entityB = seg;
				spring.stiffness = 0.25f;
				spring.restDistance = spacing;
			}
		}
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q))
		{
			setSceneComplete();
		}

		m_time += delta;

		auto& cameraTransform = ecs.getComponent<Transform>(m_mainCamera);
		vec2 mouseWorld =
			ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(Input::GetMouseX(), Input::GetMouseY()));

		if (Input::GetKeyDown(Input::E))
		{
			constexpr int FOOD_COUNT = 30;
			for (int i = 0; i < FOOD_COUNT; i++)
			{
				Entity food = ecs.createEntity();
				auto& ft = ecs.addComponent<Transform>(food);
				float ox = static_cast<float>(std::rand() % 200 - 100) * 0.06f;
				float oy = static_cast<float>(std::rand() % 200 - 100) * 0.06f;
				ft.position = vec3(mouseWorld.x + ox, mouseWorld.y + oy, 0.0f);
				ecs.setComponentDirty(ft);
				auto& fd = ecs.addComponent<Dot>(food);
				fd.materialId = 8;
				auto& frb = ecs.addComponent<RigidBody2D>(food);

				ecs.addComponent<FishFood>(food);
			}
		}

		ecs.forEach<JellyfishComponent, Transform, RigidBody2D>(
			[&](Entity bellEntity, JellyfishComponent& jf, Transform& bellT, RigidBody2D& rb)
			{
				auto& cs = ecs.getComponent<CustomShape>(jf.bellShape);
				cs.parameters[0] = bellT.position.x;
				cs.parameters[1] = bellT.position.y;
				float pulse = std::sin(m_time * jf.pulseSpeed + jf.pulsePhase);
				cs.parameters[3] = 1.0f + pulse * 0.8f;
				cs.parameters[2] = 2.5f + pulse * 0.3f;
				ecs.setComponentDirty(cs);

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

		ecs.forEach<Seaweed, CustomShape>(
			[&](Entity, Seaweed& sw, CustomShape& cs)
			{
				cs.parameters[3] = std::sin(m_time * 1.5f + sw.animationOffset) * 4.0f;
				ecs.setComponentDirty(cs);
			});

		ecs.forEach<EelComponent>(
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
				auto& headT = ecs.getComponent<Transform>(head);
				vec2 headPos(headT.position);

				auto& headRb = ecs.getComponent<RigidBody2D>(head);
				headRb.pendingContinuousForce += eel.direction * eel.speed * 120.0f;

				{
					auto foodArray = ecs.getComponentArray<FishFood>();
					auto transformArray = ecs.getComponentArray<Transform>();
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
							ecs.destroyEntity(foodEntity);

							Entity lastSeg = eel.segments.back();
							Entity prevSeg = eel.segments.size() > 1
												 ? eel.segments[eel.segments.size() - 2]
												 : eel.segments[0];
							auto& lastT = transformArray->getDataFromEntity(lastSeg);
							auto& prevT = transformArray->getDataFromEntity(prevSeg);

							vec2 tailDir = vec2(lastT.position) - vec2(prevT.position);
							float tailLen = length(tailDir);
							if (tailLen < 0.001f)
								tailDir = vec2(0.0f, -1.0f);
							else
								tailDir /= tailLen;

							Entity newSeg = ecs.createEntity();
							auto& nt = ecs.addComponent<Transform>(newSeg);
							nt.position = vec3(lastT.position.x + tailDir.x * eel.segmentSpacing,
											   lastT.position.y + tailDir.y * eel.segmentSpacing, 0.0f);

							auto& nd = ecs.addComponent<Dot>(newSeg);
							nd.materialId = static_cast<unsigned int>(eel.baseMaterial +
																	   static_cast<int>(eel.segments.size() % 4));

							ecs.addComponent<RigidBody2D>(newSeg);

							Entity springEnt = ecs.createEntity();
							auto& spring = ecs.addComponent<Spring>(springEnt);
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
					auto& segT = ecs.getComponent<Transform>(eel.segments[i]);
					auto& prevT = ecs.getComponent<Transform>(eel.segments[i - 1]);
					vec2 bodyDir = vec2(segT.position) - vec2(prevT.position);
					float bodyLen = length(bodyDir);

					if (bodyLen > 0.001f)
					{
						vec2 normal(bodyDir.y, -bodyDir.x);
						normal /= bodyLen;
						float wave = std::sin(m_time * 5.0f + eel.phaseOffset + static_cast<float>(i) * 0.6f);
						auto& segRb = ecs.getComponent<RigidBody2D>(eel.segments[i]);
						segRb.pendingContinuousForce += normal * wave * 80.0f;
					}
				}

				for (Entity seg : eel.segments)
				{
					auto& segT = ecs.getComponent<Transform>(seg);
					auto& segRb = ecs.getComponent<RigidBody2D>(seg);
					segRb.pendingContinuousForce += vec2(0.0f, 5.0f);
					if (segT.position.x < TANK_LEFT + 5.0f)
						segRb.pendingImpulseForce += vec2(2.0f, 0.0f);
					if (segT.position.x > TANK_RIGHT - 5.0f)
						segRb.pendingImpulseForce += vec2(-2.0f, 0.0f);
					if (segT.position.y < TANK_BOTTOM + 5.0f)
						segRb.pendingImpulseForce += vec2(0.0f, 2.0f);
					if (segT.position.y > TANK_TOP - 5.0f)
						segRb.pendingImpulseForce += vec2(0.0f, -2.0f);
				}
			});

		// Boids
		updateFishBoids(delta, ecs);
	}

	void updateFishBoids(float delta, ECSManager& ecs)
	{
		PROFILE_SCOPE("Boids");

		auto fishArray = ecs.getComponentArray<Fish>();
		auto foodArray = ecs.getComponentArray<FishFood>();
		auto transformArray = ecs.getComponentArray<Transform>();
		auto rbArray = ecs.getComponentArray<RigidBody2D>();

		const size_t fishCount = fishArray->getSize();
		if (fishCount == 0)
			return;

		struct FishCache
		{
			vec2 pos;
			vec2 velocity;
			float energy;
			float mateCooldown;
			float maxSpeed;
			float separationWeight;
			float alignmentWeight;
			float cohesionWeight;
			float perceptionRadius;
			Entity entity;
		};
		std::vector<FishCache> fishCache(fishCount);

		for (size_t i = 0; i < fishCount; i++)
		{
			Entity e = fishArray->getEntityAtIdx(i);
			auto& fc = fishArray->getDataAtIdx(i);
			fc.mateCooldown = std::max(0.0f, fc.mateCooldown - delta);

			auto& t = transformArray->getDataFromEntity(e);
			fishCache[i].pos = vec2(t.position);
			fishCache[i].velocity = fc.velocity;
			fishCache[i].energy = fc.energy;
			fishCache[i].mateCooldown = fc.mateCooldown;
			fishCache[i].maxSpeed = fc.maxSpeed;
			fishCache[i].separationWeight = fc.separationWeight;
			fishCache[i].alignmentWeight = fc.alignmentWeight;
			fishCache[i].cohesionWeight = fc.cohesionWeight;
			fishCache[i].perceptionRadius = fc.perceptionRadius;
			fishCache[i].entity = e;
		}

		constexpr float CELL = 10.0f;
		std::unordered_map<int, std::vector<size_t>> grid;
		grid.reserve(fishCount);

		for (size_t i = 0; i < fishCount; i++)
		{
			int cx = static_cast<int>(std::floor(fishCache[i].pos.x / CELL));
			int cy = static_cast<int>(std::floor(fishCache[i].pos.y / CELL));
			grid[cx * 73856093 ^ cy * 19349663].push_back(i);
		}

		struct FoodCache
		{
			vec2 pos;
			Entity entity;
		};
		std::vector<FoodCache> foodCache;
		size_t foodCount = foodArray->getSize();
		foodCache.reserve(foodCount);

		for (size_t i = 0; i < foodCount; i++)
		{
			auto& ff = foodArray->getDataAtIdx(i);
			if (ff.eaten)
				continue;
			Entity e = foodArray->getEntityAtIdx(i);
			if (!transformArray->hasData(e))
				continue;
			foodCache.push_back({vec2(transformArray->getDataFromEntity(e).position), e});
		}

		constexpr float EAT_RADIUS = 1.5f;
		constexpr float MATE_ENERGY = 3.0f;
		constexpr float MATE_RADIUS = 20.0f;

		for (size_t i = 0; i < fishCount; i++)
		{
			auto& fd = fishCache[i];

			vec2 separation(0.0f);
			vec2 alignment(0.0f);
			vec2 cohesion(0.0f);
			int neighborCount = 0;

			vec2 closestMatePos(0.0f);
			float closestMateDistSq = MATE_RADIUS * MATE_RADIUS;
			bool seekingMate = (fd.mateCooldown <= 0.0f && fd.energy >= MATE_ENERGY);
			float perceptionSq = fd.perceptionRadius * fd.perceptionRadius;

			int cx = static_cast<int>(std::floor(fd.pos.x / CELL));
			int cy = static_cast<int>(std::floor(fd.pos.y / CELL));

			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dy = -1; dy <= 1; dy++)
				{
					int key = (cx + dx) * 73856093 ^ (cy + dy) * 19349663;
					auto it = grid.find(key);
					if (it == grid.end())
						continue;

					for (size_t j : it->second)
					{
						if (i == j)
							continue;
						auto& other = fishCache[j];

						vec2 diff = fd.pos - other.pos;
						float distSq = diff.x * diff.x + diff.y * diff.y;

						if (distSq < perceptionSq && distSq > 0.000001f)
						{
							float dist = std::sqrt(distSq);
							separation += (diff / dist) / dist;
							alignment += other.velocity;
							cohesion += other.pos;
							neighborCount++;

							if (seekingMate && other.mateCooldown <= 0.0f && other.energy >= MATE_ENERGY &&
								distSq < closestMateDistSq)
							{
								closestMateDistSq = distSq;
								closestMatePos = other.pos;
							}
						}
						else if (seekingMate && other.mateCooldown <= 0.0f && other.energy >= MATE_ENERGY &&
								 distSq < closestMateDistSq && distSq > 0.000001f)
						{
							closestMateDistSq = distSq;
							closestMatePos = other.pos;
						}
					}
				}
			}

			vec2 boidsForce(0.0f);
			if (neighborCount > 0)
			{
				float invCount = 1.0f / static_cast<float>(neighborCount);
				separation *= invCount;
				alignment *= invCount;
				cohesion = cohesion * invCount - fd.pos;

				if (length(separation) > 0.001f)
					separation = normalize(separation) * fd.maxSpeed - fd.velocity;
				if (length(alignment) > 0.001f)
					alignment = normalize(alignment) * fd.maxSpeed - fd.velocity;
				if (length(cohesion) > 0.001f)
					cohesion = normalize(cohesion) * fd.maxSpeed - fd.velocity;

				boidsForce = separation * fd.separationWeight + alignment * fd.alignmentWeight +
							 cohesion * fd.cohesionWeight;
			}

			if (seekingMate && closestMateDistSq > 0.000001f && closestMateDistSq < MATE_RADIUS * MATE_RADIUS)
			{
				vec2 toMate = closestMatePos - fd.pos;
				vec2 mateForce = normalize(toMate) * fd.maxSpeed * 1.8f;
				boidsForce = boidsForce * 0.25f + mateForce * 0.75f;
			}

			boidsForce += vec2(0.0f, 0.1f);

			if (fd.pos.x < TANK_LEFT + 3.0f)
				boidsForce += vec2(3.0f, 0.0f);
			if (fd.pos.x > TANK_RIGHT - 3.0f)
				boidsForce += vec2(-3.0f, 0.0f);
			if (fd.pos.y < TANK_BOTTOM + 3.0f)
				boidsForce += vec2(0.0f, 3.0f);
			if (fd.pos.y > TANK_TOP - 3.0f)
				boidsForce += vec2(0.0f, -3.0f);

			fd.velocity += boidsForce * delta;
			float speed = length(fd.velocity);
			if (speed > fd.maxSpeed)
				fd.velocity = (fd.velocity / speed) * fd.maxSpeed;

			for (size_t fi = 0; fi < foodCache.size(); fi++)
			{
				auto& fc = foodCache[fi];
				if (foodArray->getDataFromEntity(fc.entity).eaten)
				{
					foodCache[fi] = foodCache.back();
					foodCache.pop_back();
					fi--;
					continue;
				}
				vec2 toFood = fc.pos - fd.pos;
				if (toFood.x * toFood.x + toFood.y * toFood.y < EAT_RADIUS * EAT_RADIUS)
				{
					fd.energy += 1.0f;
					foodArray->getDataFromEntity(fc.entity).eaten = true;
					ecs.destroyEntity(fc.entity);
					foodCache[fi] = foodCache.back();
					foodCache.pop_back();
					fi--;
				}
			}
		}

		for (size_t i = 0; i < fishCount; i++)
		{
			auto& fd = fishCache[i];
			auto& fc = fishArray->getDataAtIdx(i);
			fc.velocity = fd.velocity;
			fc.energy = fd.energy;

			auto& rb = rbArray->getDataFromEntity(fd.entity);
			rb.pendingContinuousForce += fd.velocity * 10.0f;
		}
	}

	void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		event.raw.friction *= 50.0f;

		if (std::rand() % 8 == 0)
		{
			playSound({0.015f, 150.0f + (std::rand() % 150), true, vec3(event.raw.position, 0.0f), 1});
		}

		auto eelArray = ecs.getComponentArray<EelComponent>();
		for (size_t i = 0; i < eelArray->getSize(); i++)
		{
			auto& eel = eelArray->getDataAtIdx(i);
			if (eel.segments[0] == event.entity)
			{
				eel.direction = vec2(-eel.direction.y, eel.direction.x);
				break;
			}
		}

		if (ecs.hasComponent<JellyfishComponent>(event.entity))
		{
			auto& jf = ecs.getComponent<JellyfishComponent>(event.entity);
			jf.direction = -jf.direction;
		}
	}

	void onEntityCollision(ECSManager& ecs, WeirdEngine::EntityCollisionEvent& event) override
	{
		Entity a = event.entityA;
		Entity b = event.entityB;

		if (a == INVALID_ENTITY || b == INVALID_ENTITY)
			return;

		if (std::rand() % 10 == 0)
		{
			playSound({0.01f, 300.0f + (std::rand() % 200), true, vec3(0.0f), 1});
		}

		if (ecs.hasComponent<Fish>(a) && ecs.hasComponent<Fish>(b))
		{
			auto& fishA = ecs.getComponent<Fish>(a);
			auto& fishB = ecs.getComponent<Fish>(b);

			if (fishA.energy >= 3.0f && fishB.energy >= 3.0f && fishA.mateCooldown <= 0.0f &&
				fishB.mateCooldown <= 0.0f)
			{
				fishA.energy -= 2.0f;
				fishB.energy -= 2.0f;
				fishA.mateCooldown = 5.0f;
				fishB.mateCooldown = 5.0f;

				auto& tA = ecs.getComponent<Transform>(a);
				auto& tB = ecs.getComponent<Transform>(b);
				auto& dotA = ecs.getComponent<Dot>(a);

				int numOffspring = 1 + (std::rand() % 4);
				for (int i = 0; i < numOffspring; i++)
				{
					Entity baby = ecs.createEntity();
					auto& bt = ecs.addComponent<Transform>(baby);
					float ox = static_cast<float>(std::rand() % 100 - 50) * 0.03f;
					float oy = static_cast<float>(std::rand() % 100 - 50) * 0.03f;
					bt.position = (tA.position + tB.position) * 0.5f + vec3(ox, oy, 0.0f);
					ecs.setComponentDirty(bt);

					auto& bd = ecs.addComponent<Dot>(baby);
					bd.materialId = static_cast<unsigned int>(dotA.materialId);

					auto& brb = ecs.addComponent<RigidBody2D>(baby);

					auto& bFish = ecs.addComponent<Fish>(baby);
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
};
