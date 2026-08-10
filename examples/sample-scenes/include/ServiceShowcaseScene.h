#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

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
//     void system(ECSManager& ecs, ServiceProvider& services, ...);
//
// (onRender and onImGuiRender are inlined in the scene instead, see below.)
//
// Systems never touch Scene internals: everything they need is either on the
// ECSManager& or on the ServiceProvider& passed to the callback. Even the
// scene's own state lives in the ECS (see State below): a single "state"
// entity owns it, and systems reach it through the component array.
//
// Callbacks covered: onCreate, onStart, onUpdate (4 systems), onRender,
// onImGuiRender, onPhysicsStep, onPhysicsRigidBodyCollision, onPhysicsShapeCollision,
// onEntityCollision, onEntityShapeCollision, onDestroy.
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
		float restitution = 1.2f;
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

		// Heap-allocated (see onCreateSystem): the simulation owns per-body
		// user data and deletes it when the body is removed or the scene ends.
		CharacterData* characterData = nullptr;

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
	inline State& getState(ECSManager& ecs, ServiceProvider& services)
	{
		return ecs.getComponentArray<State>()->getDataAtIdx(0);
	}

	inline Entity spawnBall(ECSManager& ecs, vec2 position)
	{
		Entity entity = ecs.createEntity();
		auto& t = ecs.addComponent<Transform>(entity);
		t.position = vec3(position, 0.0f);

		auto& dot = ecs.addComponent<Dot>(entity);
		dot.materialId = DisplaySettings::LightGray;

		auto& rb = ecs.addComponent<RigidBody2D>(entity);
		rb.velocity = vec2((std::rand() % 200 - 100) / 40.0f, 0.0f);
		ecs.setComponentDirty(rb);

		return entity;
	}

	// ---------------------------------------------------------------- onCreate
	// Runs after the ECS, materials and camera exist, before any scene file is
	// loaded and before onStart. Creates the "state" entity that owns the
	// scene's State component.
	inline void onCreateSystem(ECSManager& ecs, ServiceProvider& services)
	{
		Entity stateEntity = ecs.createEntity();
		ecs.addComponent<State>(stateEntity);
		services.tags().tag(stateEntity, "state");
		services.serialization().blacklistEntity(stateEntity);

		State& state = getState(ecs, services);
		state.initialTime = services.time().time();
		state.characterData = new CharacterData();
		std::cout << "[ServiceShowcase] onCreate at simulation time " << state.initialTime << "s" << std::endl;
	}

	// ----------------------------------------------------------------- onStart
	inline void onStartSystem(ECSManager& ecs, ServiceProvider& services)
	{
		State& state = getState(ecs, services);

		// Debug flags through the provider
		services.debug().setDebugFly(true);
		services.debug().setDebugInput(true);

		// Materials through the provider
		Material3D& floorMaterial = services.materials().createMaterial();
		floorMaterial.color = vec4(0.2f, 0.5f, 0.9f, 1.0f);
		floorMaterial.metallic = 0.5f;

		Material3D& ringMaterial = services.materials().createMaterial();
		ringMaterial.color = vec4(0.9f, 0.3f, 0.2f, 1.0f);

		// Register a custom SDF: a ring (outer circle minus inner circle)
		ShapeId ringShape;
		{
			auto x = std::make_shared<FloatVariable>(0);
			auto y = std::make_shared<FloatVariable>(1);
			auto outerRadius = std::make_shared<FloatVariable>(2);
			auto innerRadius = std::make_shared<FloatVariable>(3);

			auto outer = std::make_shared<Primitives::Circle>(x, y, outerRadius);
			auto inner =
				std::make_shared<Multiplication>(-1.0f, std::make_shared<Primitives::Circle>(x, y, innerRadius));
			auto ring = std::make_shared<Max>(outer, inner);

			ringShape = services.shapes().registerSDF(ring);

			float vars[8] = {15.0f, 20.0f, 5.0f, 4.0f};
			Entity ringEntity =
				services.shapes().addShape(ringShape, vars, ringMaterial, CombinationType::Addition, true, 0);
			ecs.getComponent<CustomShape>(ringEntity).smoothFactor = 2.0f;
		}

		// Floor
		{
			float vars[8] = {15.0f, -50.0f, 250.0f, 50.0f};
			Entity floor =
				services.shapes().addShape(DefaultShapes::BOX, vars, floorMaterial, CombinationType::SmoothAddition);
			services.tags().tag(floor, "floor");
			ecs.getComponent<CustomShape>(floor).smoothFactor = 3.0f;
		}

		// Pit: a subtraction shape; balls that roll into it fall through
		{
			float vars[8] = {30.0f, 5.0f, 4.0f};
			services.shapes().addShape(DefaultShapes::CIRCLE, vars, 0, CombinationType::Subtraction, true,
									   CustomShape::GLOBAL_GROUP);
		}

		// Camera
		ecs.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;

		// Leader ball: orbits a point (moved by FollowSystem through the
		// physics simulation)
		{
			Entity leader = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(leader);
			t.position = vec3(15.0f, 12.0f, 0.0f);

			auto& dot = ecs.addComponent<Dot>(leader);
			dot.materialId = DisplaySettings::Yellow;

			ecs.addComponent<RigidBody2D>(leader);
			services.tags().tag(leader, "leader");
		}

		// Character ball: carries per-body user data so the physics callbacks
		// can identify and tune it without touching the ECS (the physics
		// thread must not access the ECS). The data pointer is read fresh from
		// the RigidBody2D component at spawn time.
		{
			Entity character = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(character);
			t.position = vec3(15.0f, 15.0f, 0.0f);

			auto& dot = ecs.addComponent<Dot>(character);
			dot.materialId = DisplaySettings::Orange;

			auto& rb = ecs.addComponent<RigidBody2D>(character);
			services.tags().tag(character, "character");
			services.physics().setUserData(rb.simulationId, state.characterData);
		}

		// Initial ball pile
		for (int i = 0; i < 12; ++i)
		{
			float x = 8.0f + (i % 4) * 3.0f;
			float y = 28.0f + (i / 4) * 4.0f;
			spawnBall(ecs, vec2(x, y));
		}

		// UI text (screen space; blacklisted so it is never serialized)
		{
			auto makeText = [&](const char* initial, vec2 screenPosition, Entity& outEntity, int material)
			{
				outEntity = ecs.createEntity();
				services.serialization().blacklistEntity(outEntity);

				auto& t = ecs.addComponent<Transform>(outEntity);
				t.position = vec3(screenPosition, 0.0f);

				auto& text = ecs.addComponent<UITextRenderer>(outEntity);
				text.text = initial;
				text.material = material;
				text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
				text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
			};

			makeText("time 0.0s", vec2(10.0f, static_cast<float>(Display::height) - 10.0f), state.timeText,
					 DisplaySettings::LightGreen);
			makeText("entities 0", vec2(10.0f, static_cast<float>(Display::height) - 22.0f), state.entitiesText,
					 DisplaySettings::Cyan);
			makeText("collisions 0 / 0", vec2(10.0f, static_cast<float>(Display::height) - 34.0f), state.collisionsText,
					 DisplaySettings::Magenta);
			makeText("click: spawn | space: pause | arrows: gravity/damping | ctrl+s: save | ctrl+l: load | q: next",
					 vec2(10.0f, 10.0f), state.hintsText, DisplaySettings::Orange);
		}
	}

	// ----------------------------------------------------- update: spawn system
	// Periodically drops a new ball from the top of the world.
	inline void spawnSystem(ECSManager& ecs, ServiceProvider& services)
	{
		State& state = getState(ecs, services);

		state.spawnTimer += services.time().deltaTime();
		if (state.spawnTimer > 0.35f && ecs.getEntityCount() < 160)
		{
			state.spawnTimer = 0.0f;
			float x = 3.0f + static_cast<float>(std::rand() % 240) / 10.0f;
			spawnBall(ecs, vec2(x, 35.0f));
			state.ballsSpawned++;
		}
	}

	// ----------------------------------------------------- update: input system
	inline void inputSystem(ECSManager& ecs, ServiceProvider& services)
	{
		State& state = getState(ecs, services);

		// Scene transition through the provider
		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		// Pause / resume through the provider
		if (Input::GetKeyDown(Input::Space))
		{
			if (services.physics().isPaused())
				services.physics().resume();
			else
				services.physics().pause();
		}

		// Real-time physics settings through the provider
		if (Input::GetKeyDown(Input::Up))
		{
			state.gravity = std::clamp(state.gravity + 1.0f, -30.0f, 0.0f);
			services.physics().setGravity(state.gravity);
		}
		if (Input::GetKeyDown(Input::Down))
		{
			state.gravity = std::clamp(state.gravity - 1.0f, -30.0f, 0.0f);
			services.physics().setGravity(state.gravity);
		}
		if (Input::GetKeyDown(Input::Left))
		{
			state.damping = std::max(0.0f, state.damping - 0.05f);
			services.physics().setDamping(state.damping);
		}
		if (Input::GetKeyDown(Input::Right))
		{
			state.damping += 0.05f;
			services.physics().setDamping(state.damping);
		}

		// Spawn a ball where the mouse points
		if (Input::GetMouseButtonDown(Input::LeftClick) && !Input::isUIClick())
		{
			auto& cameraTransform = ecs.getComponent<Transform>(services.render().getCameraEntity());
			vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(
				cameraTransform, vec2(Input::GetMouseX(), Input::GetMouseY()));
			spawnBall(ecs, mouseWorld);
			state.ballsSpawned++;
		}

		// Serialization through the provider
		if (Input::GetKeyDown(Input::S) && Input::GetKey(Input::LeftCtrl))
		{
			services.serialization().saveScene(ASSETS_PATH "scenes/service_showcase.weird");
			std::cout << "[ServiceShowcase] scene saved" << std::endl;
		}
		if (Input::GetKeyDown(Input::L) && Input::GetKey(Input::LeftCtrl))
		{
			// blacklistEntities = true: entities loaded from disk are excluded
			// from future saves, so saving again does not duplicate them.
			TagMap loaded = services.serialization().loadWeirdFile(ASSETS_PATH "scenes/service_showcase.weird", true);
			for (const auto& [name, entity] : loaded)
				std::cout << "[ServiceShowcase] loaded tag '" << name << "' -> entity " << entity << std::endl;
		}
	}

	// ----------------------------------------------------- update: follow system
	// Orbits the "leader" ball around a point by writing its velocity straight
	// into the physics simulation through the provider.
	inline void followSystem(ECSManager& ecs, ServiceProvider& services)
	{
		State& state = getState(ecs, services);

		Entity leader = services.tags().getEntityByTag("leader");
		if (leader == INVALID_ENTITY)
			return;

		state.leaderAngle += services.time().deltaTime() * 1.5f;

		glm::vec2 center(15.0f, 18.0f);
		glm::vec2 target = center + 6.0f * glm::vec2(std::cos(state.leaderAngle), std::sin(state.leaderAngle));

		auto& rb = ecs.getComponent<RigidBody2D>(leader);
		glm::vec2 current = glm::vec2(ecs.getComponent<Transform>(leader).position);
		services.physics().sim().setVelocity(rb.simulationId, (target - current) * 2.0f);
	}

	// -------------------------------------------------------- update: ui system
	inline void uiSystem(ECSManager& ecs, ServiceProvider& services)
	{
		State& state = getState(ecs, services);

		char buffer[64];

		auto& timeText = ecs.getComponent<UITextRenderer>(state.timeText);
		std::snprintf(buffer, sizeof(buffer), "time %.1fs", services.time().time());
		timeText.text = buffer;
		ecs.setComponentDirty(timeText);

		auto& entitiesText = ecs.getComponent<UITextRenderer>(state.entitiesText);
		std::snprintf(buffer, sizeof(buffer), "entities %d (balls spawned: %d)", services.ecs().getEntityCount(),
					  state.ballsSpawned);
		entitiesText.text = buffer;
		ecs.setComponentDirty(entitiesText);

		auto& collisionsText = ecs.getComponent<UITextRenderer>(state.collisionsText);
		std::snprintf(buffer, sizeof(buffer), "collisions %d body / %d shape", state.entityCollisions,
					  state.shapeCollisions);
		collisionsText.text = buffer;
		ecs.setComponentDirty(collisionsText);
	}

	// ------------------------------------------------------------ onPhysicsStep
	// Physics thread. Simulation-coupled logic only: no ECS access here (the
	// engine does not allow touching the ECS from the physics thread), so the
	// step counter is a local static rather than a component.
	// Applies a gentle "wind" to every body every 60 steps. Physics-thread
	// systems only receive the Simulation2D& (no ECS, no ServiceProvider).
	inline void onPhysicsStepSystem(Simulation2D& simulation)
	{
		static int stepCounter = 0;
		if (++stepCounter % 60 != 0)
			return;

		for (SimulationID id = 0; id < simulation.getSize(); ++id)
			simulation.addImpulseForce(id, vec2(0.5f, 0.0f));

		// Per-body user data: iterate only the bodies that opted in, and
		// branch on the type discriminator. No ECS access needed here.
		simulation.forEachUserData(
			[&](SimulationID id, BodyUserData& data)
			{
				if (data.type == CharacterData::TYPE)
				{
					// Give the character a little extra lift each gust
					simulation.addImpulseForce(id, vec2(0.0f, 4.0f));
				}
			});
	}

	// -------------------------------------------------------------- onPhysicsRigidBodyCollision
	// Physics thread. Body-body collisions: push the pair apart based on their
	// relative velocity.
	inline void onPhysicsRigidBodyCollisionSystem(Simulation2D& simulation, CollisionEvent& event)
	{
		vec2 va = simulation.getPhysicsVelocity(event.bodyA);
		vec2 vb = simulation.getPhysicsVelocity(event.bodyB);

		vec2 separation = (va - vb) * 0.05f;
		simulation.addImpulseForce(event.bodyA, -separation);
		simulation.addImpulseForce(event.bodyB, separation);
	}

	// --------------------------------------------------------- onPhysicsShapeCollision
	// Physics thread. Shape collisions: bounce the body off the shape normal
	// when the penetration is deep enough. The character (identified via its
	// per-body user data) gets a bouncier, more slippery response by tuning
	// the event before the solver reads it.
	inline void onPhysicsShapeCollisionSystem(Simulation2D& simulation, ShapeCollisionEvent& event)
	{
		if (event.state == CollisionState::START && event.penetration > 0.1f)
		{
			simulation.addImpulseForce(event.body, event.normal * (2.0f + event.penetration * 10.0f));
		}

		// Type-checked cast: nullptr unless this body carries CharacterData
		if (auto* data = simulation.getUserDataAs<CharacterData>(event.body))
		{
			event.absortion *= 0.5f; // less normal damping = bouncier
			event.friction *= 0.5f;	 // slippery character
		}
	}

	// ------------------------------------------------------- onEntityCollision
	// Main thread. Body-body collisions mapped to entities: count them, flash
	// the colliding ball and play a sound through the provider.
	inline void onEntityCollisionSystem(ECSManager& ecs, ServiceProvider& services, EntityCollisionEvent& event)
	{
		State& state = getState(ecs, services);
		state.entityCollisions++;

		if (event.entityA != INVALID_ENTITY && ecs.hasComponent<Dot>(event.entityA))
		{
			auto& dot = ecs.getComponent<Dot>(event.entityA);
			dot.materialId = DisplaySettings::Orange;
			ecs.setComponentDirty(dot);
		}

		services.audio().playSound({0.05f, 300.0f, false, vec3(0.0f), 1});
	}

	// --------------------------------------------------- onEntityShapeCollision
	// Main thread. Shape collisions mapped to entities: count them and play a
	// spatial sound at the contact point.
	inline void onEntityShapeCollisionSystem(ECSManager& ecs, ServiceProvider& services,
											 EntityShapeCollisionEvent& event)
	{
		State& state = getState(ecs, services);
		state.shapeCollisions++;

		if (event.entity != INVALID_ENTITY)
		{
			float frequency = 200.0f + static_cast<float>(state.shapeCollisions % 40) * 5.0f;
			services.audio().playSound({0.04f, frequency, true, vec3(event.raw.position, 0.0f), 1});
		}
	}

	// ---------------------------------------------------------------- onDestroy
	inline void onDestroySystem(ECSManager& ecs, ServiceProvider& services)
	{
		std::cout << "[ServiceShowcase] scene destroyed at " << services.time().time() << "s" << std::endl;

		State& state = getState(ecs, services);
		state.ballsSpawned = 0;
		state.entityCollisions = 0;
		state.shapeCollisions = 0;
	}
} // namespace ServiceShowcase

class ServiceShowcaseScene : public Scene2D
{
public:
	ServiceShowcaseScene() = default;

private:
	// Most callbacks are thin wrappers around a system. The ServiceProvider
	// only reaches callbacks through their `services` parameter (getServices()
	// is private); the scene owns no state at all (it lives in the State
	// component on the "state" entity). onRender and onImGuiRender are
	// inlined below instead of using systems.

	void onCreate(ECSManager& ecs, ServiceProvider& services) override
	{
		ServiceShowcase::onCreateSystem(ecs, services);
	}

	void onStart(ECSManager& ecs, ServiceProvider& services) override
	{
		ServiceShowcase::onStartSystem(ecs, services);
	}

	void onUpdate(ECSManager& ecs, ServiceProvider& services) override
	{
		ServiceShowcase::spawnSystem(ecs, services);
		ServiceShowcase::inputSystem(ecs, services);
		ServiceShowcase::followSystem(ecs, services);
		ServiceShowcase::uiSystem(ecs, services);
	}

	void onEntityCollision(ECSManager& ecs, ServiceProvider& services, EntityCollisionEvent& event) override
	{
		ServiceShowcase::onEntityCollisionSystem(ecs, services, event);
	}

	void onEntityShapeCollision(ECSManager& ecs, ServiceProvider& services, EntityShapeCollisionEvent& event) override
	{
		ServiceShowcase::onEntityShapeCollisionSystem(ecs, services, event);
	}

	void onDestroy(ECSManager& ecs, ServiceProvider& services) override
	{
		ServiceShowcase::onDestroySystem(ecs, services);
	}

	// Don't use systems for these callbacks (they are inlined below).
	// Note the firing rules: onRender only fires for 3D / both render modes,
	// while onImGuiRender fires for every scene, 2D and 3D alike (it is just
	// the debug UI).
	void onRender(WeirdRenderer::RenderTarget& renderTarget) override
	{
		static bool logged = false;
		if (!logged)
		{
			logged = true;
			std::cout << "[ServiceShowcase] onRender (3D render path)" << std::endl;
		}
	}

	void onImGuiRender(ECSManager& ecs, ServiceProvider& services) override
	{
		auto& state = ServiceShowcase::getState(ecs, services);

		ImGui::Text("Time: %.2fs", services.time().time());
		ImGui::Text("Entities: %d", services.ecs().getEntityCount());
		ImGui::Text("Gravity: %.1f | Damping: %.2f | Physics %s", state.gravity, state.damping,
					services.physics().isPaused() ? "paused" : "running");
		ImGui::Text("Collisions: %d body / %d shape", state.entityCollisions, state.shapeCollisions);
		ImGui::Text("Balls spawned: %d", state.ballsSpawned);

		ImGui::Separator();
		ImGui::Text("Left click: spawn ball | Space: pause/resume");
		ImGui::Text("Up/Down: gravity | Left/Right: damping");
		ImGui::Text("Ctrl+S: save scene | Ctrl+L: load scene | Q: next scene");
	}

	// Physics thread callbacks. Fire on the physics thread mid-step; no ECS
	// access here. They delegate to Simulation2D-only systems.
	void onPhysicsStep(Simulation2D& simulation) override
	{
		ServiceShowcase::onPhysicsStepSystem(simulation);
	}

	void onPhysicsRigidBodyCollision(Simulation2D& simulation, CollisionEvent& event) override
	{
		ServiceShowcase::onPhysicsRigidBodyCollisionSystem(simulation, event);
	}

	void onPhysicsShapeCollision(Simulation2D& simulation, ShapeCollisionEvent& event) override
	{
		ServiceShowcase::onPhysicsShapeCollisionSystem(simulation, event);
	}
};
