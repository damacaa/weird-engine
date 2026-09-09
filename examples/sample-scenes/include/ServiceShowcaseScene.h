#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "weird-renderer/audio/SdfSong.h"
#include <weird-engine.h>

#include "globals.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;

// ============================================================================
// Service showcase scene.
//
// Demonstrates the new system + ServiceProvider API: most Scene callbacks are
// thin wrappers that delegate to a plain free function (a "system") of the
// form:
//
//     void system(Registry& registry, ServiceProvider& services, ...);
//
// (onRender and the physics-thread callbacks are inlined in the scene
// instead, see below: the dispatcher is deliberately not involved there.)
//
// Systems never touch Scene internals: everything they need is either on the
// Registry& or on the ServiceProvider& passed to the callback. Even the
// scene's own state lives in the ECS (see State below): a single "state"
// entity owns it, and systems reach it through the component array.
//
// Registered as systems: onCreate, onStart, onUpdate (4 systems),
// onImGuiRender, onEntityCollision, onEntityShapeCollision, onDestroy.
// Inlined overrides: onRender and the physics-thread callbacks
// (onPhysicsStep, onPhysicsRigidBodyCollision, onPhysicsShapeCollision).
//
// Controls:
//   Left click     spawn a ball at the cursor
//   Space          pause / resume the physics simulation
//   Up / Down      gravity up / down (live)
//   Left / Right   damping down / up (live)
//   Ctrl+S         save the scene to assets/scenes/service_showcase.weird
//   Ctrl+L         load the saved scene into the current one
//   Q              go to the next scene
// ============================================================================

namespace ServiceShowcase
{
	// Game-defined per-body user data: derives from BodyUserData and sets a
	// type discriminator so the physics callbacks can cast safely (see
	// getUserDataAs<T>).
	struct CharacterData : BodyUserData
	{
		static constexpr int TYPE = 1;

		// The inherited `type` member must match TYPE or every
		// getUserDataAs<T>() / forEachUserData() check will fail.
		CharacterData()
		{
			type = TYPE;
		}

		float restitution = 1.2f;
		float jumpStrength = 5.0f;
	};

	// Scene state as an ECS component: attached to a single "state" entity
	// created by onCreateSystem. This is the ECS-native way for systems to
	// share state instead of passing a struct around.
	struct State
	{
		Entity timeText = INVALID_ENTITY;
		Entity entitiesText = INVALID_ENTITY;
		Entity collisionsText = INVALID_ENTITY;
		Entity hintsText = INVALID_ENTITY;

		float spawnTimer = 0.0f;
		float leaderAngle = 0.0f;
		int ballsSpawned = 0;
		float gravity = -9.8f;
		float damping = 0.001f;
		float initialTime = 0.0f;

		// Each counter is only touched from the main thread (collision
		// callbacks); plain ints are fine. Atomicity would require a custom
		// component manager since components must be copyable for the ECS
		// storage.
		int entityCollisions = 0;
		int shapeCollisions = 0;
	};

	// State entity lookup: there is exactly one State component in the scene
	// (created by onCreateSystem), so it always lives at index 0 of the State
	// component array.
	inline State& getState(Registry& registry, ServiceProvider& services)
	{
		return registry.getComponentArray<State>()->getDataAtIdx(0);
	}

	inline Entity spawnBall(Registry& registry, ServiceProvider& services, vec2 position)
	{
		Entity entity = registry.createEntity();
		auto& t = registry.addComponent<Transform>(entity);
		t.position = vec3(position, 0.0f);

		auto& dot = registry.addComponent<Dot>(entity);
		dot.materialId = services.materials2D().getHandle("ball").id;

		auto& rb = registry.addComponent<RigidBody2D>(entity);
		rb.velocity = vec2((std::rand() % 200 - 100) / 40.0f, 0.0f);
		registry.setComponentDirty(rb);

		return entity;
	}

	// ---------------------------------------------------------------- onCreate
	// Runs after the ECS, materials and camera exist, before any scene file is
	// loaded and before onStart. Creates the "state" entity that owns the
	// scene's State component.
	inline void onCreateSystem(Registry& registry, ServiceProvider& services)
	{
		Entity stateEntity = registry.createEntity();
		registry.addComponent<State>(stateEntity);
		services.tags().tag(stateEntity, "state");
		services.serialization().blacklistEntity(stateEntity);

		State& state = getState(registry, services);
		state.initialTime = services.time().time();
		std::cout << "[ServiceShowcase] onCreate at simulation time " << state.initialTime << "s" << std::endl;
	}

	inline std::shared_ptr<WeirdRenderer::SdfSong> createSceneSong()
	{
		using namespace SDF;
		Vec2Expr p = point();

		// Radiant multi-pointed star shape (scaled 10x for UI)
		Expr star = sdStar(p, 25.0f, 12.0f, 6.0f, 0.0f);
		Expr core = sdCircle(p, 14.0f);
		Expr showcaseShape = sdfSmoothUnion(star, core, 4.0f);

		return WeirdRenderer::SdfSong::create("showcase", showcaseShape);
	}

	// ----------------------------------------------------------------- onStart
	inline void onStartSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry, services);

		// Debug flags through the provider
		services.debug().setDebugFly(true);
		services.debug().setDebugInput(true);

		// Initialize audio with scene-defined showcase song
		services.audio().setSong(createSceneSong());

		// Materials through the provider
		Material2D& floorMaterial = services.materials2D().createMaterial("floor");
		floorMaterial.color = ColorPalette::LightGray;

		Material2D& ringMaterial = services.materials2D().createMaterial("ring");
		ringMaterial.color = ColorPalette::Red;

		Material2D& ballMat = services.materials2D().createMaterial("ball");
		ballMat.color = ColorPalette::LightGray;

		Material2D& pitMat = services.materials2D().createMaterial("pit");
		pitMat.color = ColorPalette::Black;

		Material2D& leaderMat = services.materials2D().createMaterial("leader");
		leaderMat.color = ColorPalette::Yellow;

		Material2D& charMat = services.materials2D().createMaterial("character");
		charMat.color = ColorPalette::Blue;

		Material2D& timeMat = services.materials2D().createMaterial("ui_time");
		timeMat.color = ColorPalette::LightGreen;

		Material2D& entMat = services.materials2D().createMaterial("ui_entities");
		entMat.color = ColorPalette::Cyan;

		Material2D& colMat = services.materials2D().createMaterial("ui_collisions");
		colMat.color = ColorPalette::Magenta;

		Material2D& hintsMat = services.materials2D().createMaterial("ui_hints");
		hintsMat.color = ColorPalette::Orange;

		// Register a custom SDF: a ring (outer circle minus inner circle)
		ShapeId ringShape;
		{
			using namespace SDF;
			auto p = translate(point(), {var(0), var(1)});
			auto ring = sdfSubtract(sdCircle(p, var(2)), sdCircle(p, var(3)));

			ringShape = services.shapes().registerSDF(ring);

			Entity ringEntity = services.shapes().addShape({.shapeId = ringShape,
															.variables = {15.0f, 20.0f, 5.0f, 4.0f},
															.material = ringMaterial,
															.combination = CombinationType::Addition,
															.hasCollision = true,
															.group = 0});
			registry.getComponent<CustomShape>(ringEntity).smoothFactor = 2.0f;
		}

		// Floor
		Entity floor = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
												   .variables = {{Primitives::Box::POS_X, 15.0f},
																 {Primitives::Box::POS_Y, -50.0f},
																 {Primitives::Box::SIZE_X, 250.0f},
																 {Primitives::Box::SIZE_Y, 50.0f}},
												   .material = floorMaterial,
												   .combination = CombinationType::SmoothAddition});
		services.tags().tag(floor, "floor");
		registry.getComponent<CustomShape>(floor).smoothFactor = 3.0f;

		// Pit: a subtraction shape; balls that roll into it fall through
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 30.0f},
												  {Primitives::Circle::POS_Y, 5.0f},
												  {Primitives::Circle::RADIUS, 4.0f}},
									.material = pitMat,
									.combination = CombinationType::Subtraction,
									.hasCollision = true,
									.group = CustomShape::GLOBAL_GROUP});

		// Camera
		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;

		// Leader ball: orbits a point (moved by FollowSystem through the
		// physics simulation)
		{
			Entity leader = registry.createEntity();
			auto& t = registry.addComponent<Transform>(leader);
			t.position = vec3(15.0f, 12.0f, 0.0f);

			auto& dot = registry.addComponent<Dot>(leader);
			dot.materialId = leaderMat.id;

			registry.addComponent<RigidBody2D>(leader);
			services.tags().tag(leader, "leader");
		}

		// Character ball: carries per-body user data so the physics callbacks
		// can identify and tune it without touching the ECS (the physics
		// thread must not access the ECS). Ownership of the data is handed off
		// to the simulation; the callbacks reach it through the simulation id
		// stored in the RigidBody2D component.
		{
			Entity character = registry.createEntity();
			auto& t = registry.addComponent<Transform>(character);
			t.position = vec3(15.0f, 15.0f, 0.0f);

			auto& dot = registry.addComponent<Dot>(character);
			dot.materialId = charMat.id;

			auto& rb = registry.addComponent<RigidBody2D>(character);
			services.tags().tag(character, "character");

			// Configure the data before handing ownership to the simulation;
			// std::move() empties the local unique_ptr, so the only way to
			// reach the data afterwards is getUserDataAs<T>().
			auto characterData = std::make_unique<CharacterData>();
			characterData->jumpStrength = 10.0f;
			services.physics().setUserData(rb.simulationId, std::move(characterData));

			// Reading and modifying the data after the handoff: no cached
			// pointer is kept, everything goes through the physics service.
			services.physics().getUserDataAs<CharacterData>(rb.simulationId)->restitution = 2.0f;
		}

		// Initial ball pile
		for (int i = 0; i < 12; ++i)
		{
			float x = 8.0f + (i % 4) * 3.0f;
			float y = 28.0f + (i / 4) * 4.0f;
			spawnBall(registry, services, vec2(x, y));
		}

		// UI text (screen space; blacklisted so it is never serialized)
		{
			auto makeText = [&](const char* initial, vec2 screenPosition, Entity& outEntity, Material2DHandle material)
			{
				outEntity = registry.createEntity();
				services.serialization().blacklistEntity(outEntity);

				auto& t = registry.addComponent<Transform>(outEntity);
				t.position = vec3(screenPosition, 0.0f);

				auto& text = registry.addComponent<UITextRenderer>(outEntity);
				text.text = initial;
				text.material = material;
				text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
				text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
			};

			makeText("time 0.0s", vec2(10.0f, static_cast<float>(Display::height) - 10.0f), state.timeText, timeMat);
			makeText("entities 0", vec2(10.0f, static_cast<float>(Display::height) - 22.0f), state.entitiesText,
					 entMat);
			makeText("collisions 0 / 0", vec2(10.0f, static_cast<float>(Display::height) - 34.0f), state.collisionsText,
					 colMat);
			makeText("click: spawn | space: pause | arrows: gravity/damping | ctrl+s: save | ctrl+l: load | q: next",
					 vec2(10.0f, 10.0f), state.hintsText, hintsMat);
		}
	}

	// ----------------------------------------------------- update: spawn system
	// Periodically drops a new ball from the top of the world.
	inline void spawnSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry, services);

		state.spawnTimer += services.time().deltaTime();
		if (state.spawnTimer > 0.35f && registry.getEntityCount() < 160)
		{
			state.spawnTimer = 0.0f;
			float x = 3.0f + static_cast<float>(std::rand() % 240) / 10.0f;
			spawnBall(registry, services, vec2(x, 35.0f));
			state.ballsSpawned++;
		}
	}

	// ----------------------------------------------------- update: input system
	inline void inputSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry, services);

		// Scene transition through the provider
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		// Pause / resume through the provider
		if (services.input().getKeyDown(Input::Space))
		{
			if (services.physics().isPaused())
				services.physics().resume();
			else
				services.physics().pause();
		}

		// Real-time physics settings through the provider
		if (services.input().getKeyDown(Input::Up))
		{
			state.gravity = std::clamp(state.gravity + 1.0f, -30.0f, 0.0f);
			services.physics().setGravity(state.gravity);
		}
		if (services.input().getKeyDown(Input::Down))
		{
			state.gravity = std::clamp(state.gravity - 1.0f, -30.0f, 0.0f);
			services.physics().setGravity(state.gravity);
		}
		if (services.input().getKeyDown(Input::Left))
		{
			state.damping = std::max(0.0f, state.damping - 0.05f);
			services.physics().setDamping(state.damping);
		}
		if (services.input().getKeyDown(Input::Right))
		{
			state.damping += 0.05f;
			services.physics().setDamping(state.damping);
		}

		// Spawn a ball where the mouse points
		if (services.input().getMouseButtonDown(Input::LeftClick) && !services.input().isUIClick())
		{
			auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(
				cameraTransform, vec2(services.input().getMouseX(), services.input().getMouseY()));
			spawnBall(registry, services, mouseWorld);
			state.ballsSpawned++;
		}

		// Serialization through the provider
		if (services.input().getKeyDown(Input::S) && services.input().getKey(Input::LeftCtrl))
		{
			services.serialization().saveScene(services.resources().assetPath("scenes/service_showcase.weird"));
			std::cout << "[ServiceShowcase] scene saved" << std::endl;
		}
		if (services.input().getKeyDown(Input::L) && services.input().getKey(Input::LeftCtrl))
		{
			// blacklistEntities = true: entities loaded from disk are excluded
			// from future saves, so saving again does not duplicate them.
			TagMap loaded = services.serialization().loadWeirdFile(
				services.resources().assetPath("scenes/service_showcase.weird"), true);
			for (const auto& [name, entity] : loaded)
				std::cout << "[ServiceShowcase] loaded tag '" << name << "' -> entity " << entity << std::endl;
		}
	}

	// ----------------------------------------------------- update: follow system
	// Orbits the "leader" ball around a point by writing its velocity straight
	// into the physics simulation through the provider.
	inline void followSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry, services);

		Entity leader = services.tags().getEntityByTag("leader");
		if (leader == INVALID_ENTITY)
			return;

		state.leaderAngle += services.time().deltaTime() * 1.5f;

		glm::vec2 center(15.0f, 18.0f);
		glm::vec2 target = center + 6.0f * glm::vec2(std::cos(state.leaderAngle), std::sin(state.leaderAngle));

		auto& rb = registry.getComponent<RigidBody2D>(leader);
		glm::vec2 current = glm::vec2(registry.getComponent<Transform>(leader).position);
		rb.velocity = (target - current) * 2.0f;
		registry.setComponentDirty(rb);
	}

	// -------------------------------------------------------- update: ui system
	inline void uiSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry, services);

		char buffer[64];

		auto& timeText = registry.getComponent<UITextRenderer>(state.timeText);
		std::snprintf(buffer, sizeof(buffer), "time %.1fs", services.time().time());
		timeText.text = buffer;
		registry.setComponentDirty(timeText);

		auto& entitiesText = registry.getComponent<UITextRenderer>(state.entitiesText);
		std::snprintf(buffer, sizeof(buffer), "entities %d (balls spawned: %d)", services.registry().getEntityCount(),
					  state.ballsSpawned);
		entitiesText.text = buffer;
		registry.setComponentDirty(entitiesText);

		auto& collisionsText = registry.getComponent<UITextRenderer>(state.collisionsText);
		std::snprintf(buffer, sizeof(buffer), "collisions %d body / %d shape", state.entityCollisions,
					  state.shapeCollisions);
		collisionsText.text = buffer;
		registry.setComponentDirty(collisionsText);
	}

	// ------------------------------------------------------- onEntityCollision
	// Main thread. Body-body collisions mapped to entities: count them, flash
	// the colliding ball and play a sound through the provider.
	inline void onEntityCollisionSystem(Registry& registry, ServiceProvider& services, EntityCollisionEvent& event)
	{
		State& state = getState(registry, services);
		state.entityCollisions++;

		// Flash the colliding ball orange, but keep the character's identity
		// color: it is identified through its per-body user data.
		if (event.entityA != INVALID_ENTITY && registry.hasComponent<Dot>(event.entityA) &&
			registry.hasComponent<RigidBody2D>(event.entityA))
		{
			RigidBody2D& rb = registry.getComponent<RigidBody2D>(event.entityA);
			if (services.physics().getUserDataAs<CharacterData>(rb.simulationId) == nullptr)
			{
				auto& dot = registry.getComponent<Dot>(event.entityA);
				dot.materialId = services.materials2D().getHandle("ui_hints").id;
				registry.setComponentDirty(dot);
			}
		}

		services.audio().playSound({0.05f, 300.0f, false, vec3(0.0f), 1});
	}

	// --------------------------------------------------- onEntityShapeCollision
	// Main thread. Shape collisions mapped to entities: count them and play a
	// spatial sound at the contact point.
	inline void onEntityShapeCollisionSystem(Registry& registry, ServiceProvider& services,
											 EntityShapeCollisionEvent& event)
	{
		State& state = getState(registry, services);
		state.shapeCollisions++;

		if (event.entity != INVALID_ENTITY)
		{
			float frequency = 200.0f + static_cast<float>(state.shapeCollisions % 40) * 5.0f;
			services.audio().playSound({0.04f, frequency, true, vec3(event.raw.position, 0.0f), 1});
		}
	}

	// ---------------------------------------------------------------- onDestroy
	inline void onDestroySystem(Registry& registry, ServiceProvider& services)
	{
		std::cout << "[ServiceShowcase] scene destroyed at " << services.time().time() << "s" << std::endl;

		State& state = getState(registry, services);
		state.ballsSpawned = 0;
		state.entityCollisions = 0;
		state.shapeCollisions = 0;
	}
} // namespace ServiceShowcase

class ServiceShowcaseScene : public Scene2D
{
public:
	ServiceShowcaseScene()
	{
		addCreateSystem(ServiceShowcase::onCreateSystem);
		addStartSystem(ServiceShowcase::onStartSystem);

		// Multiple systems for the same stage run sequentially!
		addUpdateSystem(ServiceShowcase::spawnSystem);
		addUpdateSystem(ServiceShowcase::inputSystem);
		addUpdateSystem(ServiceShowcase::followSystem);
		addUpdateSystem(ServiceShowcase::uiSystem);

		addEntityCollisionSystem(ServiceShowcase::onEntityCollisionSystem);
		addEntityShapeCollisionSystem(ServiceShowcase::onEntityShapeCollisionSystem);

		addDestroySystem(ServiceShowcase::onDestroySystem);

#ifndef WEIRD_DISABLE_IMGUI
		addImGuiRenderSystem(
			[](Registry& registry, ServiceProvider& services)
			{
				auto& state = ServiceShowcase::getState(registry, services);

				ImGui::Text("Time: %.2fs", services.time().time());
				ImGui::Text("Entities: %d", services.registry().getEntityCount());
				ImGui::Text("Gravity: %.1f | Damping: %.2f | Physics %s", state.gravity, state.damping,
							services.physics().isPaused() ? "paused" : "running");
				ImGui::Text("Collisions: %d body / %d shape", state.entityCollisions, state.shapeCollisions);
				ImGui::Text("Balls spawned: %d", state.ballsSpawned);

				ImGui::Separator();
				ImGui::Text("Left click: spawn ball | Space: pause/resume");
				ImGui::Text("Up/Down: gravity | Left/Right: damping");
				ImGui::Text("Ctrl+S: save scene | Ctrl+L: load scene | Q: next scene");
			});
#endif
	}

private:
	void onRender(Registry& registry, ServiceProvider& services, WeirdRenderer::RenderTarget& renderTarget) override
	{
		static bool logged = false;
		if (!logged)
		{
			logged = true;
			std::cout << "[ServiceShowcase] onRender (3D render path)" << std::endl;
		}
	}

	// Physics thread callbacks. Fire on the physics thread mid-step; no ECS
	// access here. They delegate to Simulation2D-only systems.
	void onPhysicsStep(Simulation2D& simulation) override
	{
		// ------------------------------------------------------------ onPhysicsStep
		// Physics thread. Simulation-coupled logic only: no ECS access here (the
		// engine does not allow touching the ECS from the physics thread), so the
		// step counter is a local static rather than a component.
		// Every 120 steps, applies a wind gust that alternates direction.
		// Physics-thread systems only receive the Simulation2D& (no ECS, no
		// ServiceProvider).
		static int stepCounter = 0;
		if (++stepCounter % 120 != 0)
			return;

		static float direction = 1.0f;
		direction *= -1.0f;

		for (SimulationID id = 0; id < simulation.getSize(); ++id)
			simulation.addImpulseForce(id, vec2(2.5f * direction, 0.0f));

		// Per-body user data: iterate only the bodies that opted in, and
		// branch on the type discriminator. No ECS access needed here.
		simulation.forEachUserData(
			[&](SimulationID id, BodyUserData& data)
			{
				if (data.type == ServiceShowcase::CharacterData::TYPE)
				{
					auto characterData = simulation.getUserDataAs<ServiceShowcase::CharacterData>(id);

					// Kick the character into a visible hop each gust.
					// massIndependent = true: jumpStrength is the delta-v in
					// m/s, regardless of the body's mass.
					simulation.addImpulseForce(id, vec2(0.0f, characterData->jumpStrength), true);
				}
			});
	}

	void onPhysicsRigidBodyCollision(Simulation2D& simulation, PhysicsCollisionEvent& event) override
	{
		// -------------------------------------------------------------- onPhysicsRigidBodyCollision
		// Physics thread. Body-body collisions: push the pair apart based on their
		// relative velocity.
		vec2 va = simulation.getPhysicsVelocity(event.bodyA);
		vec2 vb = simulation.getPhysicsVelocity(event.bodyB);

		vec2 separation = (va - vb) * 0.05f;
		simulation.addImpulseForce(event.bodyA, -separation);
		simulation.addImpulseForce(event.bodyB, separation);
	}

	void onPhysicsShapeCollision(Simulation2D& simulation, PhysicsShapeCollisionEvent& event) override
	{
		// --------------------------------------------------------- onPhysicsShapeCollision
		// Physics thread. Shape collisions: bounce the body off the shape normal
		// when the penetration is deep enough. The character (identified via its
		// per-body user data) gets a bouncier, more slippery response by tuning
		// the event before the solver reads it.
		if (event.state == CollisionState::START && event.penetration > 0.1f)
		{
			simulation.addImpulseForce(event.body, event.normal * (2.0f + event.penetration * 10.0f));
		}

		// Type-checked cast: nullptr unless this body carries CharacterData
		if (auto* data = simulation.getUserDataAs<ServiceShowcase::CharacterData>(event.body))
		{
			// absortion damps the normal axis (more damping = less bounce), so
			// higher restitution = bouncier. At restitution = 2.0 (set in
			// onStartSystem) this equals the previous hard-coded 0.5x.
			event.absortion *= 1.0f / data->restitution;
			event.friction *= 0.5f; // slippery character
		}
	}
};
