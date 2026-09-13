#pragma once

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

#include <filesystem>

#include "globals.h"

using namespace WeirdEngine;

struct Foot
{
	Foot() {};

	vec2 direction = vec2(1.0f, 0.0f);
	vec2 initialPos = vec2(0.0f, 0.0f);
	float forceMagnitude = 1.0f;
	bool directionChanged = false;
	float t = 0.0f;
	bool onFloor = false;
	bool stepStarted = false;
};

class WalkScene : public Scene2D
{
public:
	WalkScene() {};

	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		Vec2Expr p = SDF::point();

		// Footstep path shape: rounded box path with stepping stone circles (scaled 10x for UI)
		Expr path = sdBox(p, Vec2Expr(30.0f, 6.0f));
		Expr stone1 = sdCircle(p + Vec2Expr(15.0f, 5.0f), 7.0f);
		Expr stone2 = sdCircle(p - Vec2Expr(15.0f, -5.0f), 7.0f);
		Expr walkShape = sdfSmoothUnion(sdfSmoothUnion(path, stone1, 4.0f), stone2, 4.0f);

		return WeirdAudio::SdfSong::create("walk", walkShape);
	}

private:
	Entity m_head;

	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio with scene-defined walk song
		services.audio().setSong(createSceneSong(), {.mode = SongVisualizationMode::UI});

		auto& background = services.render().getBackground();
		background.type = BackgroundType::Sky;
		background.primaryColor = vec4(0.2f, 0.55f, 0.9f, 1.0f);
		background.secondaryColor = vec4(0.4f, 0.75f, 0.85f, 1.0f);
		background.scale = 0.2f;

		for (size_t i = 0; i < ColorPalette::Default.size() && i < 16; ++i)
		{
			auto& m = services.materials2D().get(static_cast<uint16_t>(i));
			m.color = ColorPalette::Default[i];
		}

		auto tags = services.serialization().loadWeirdFile(services.resources().assetPath("man.weird"));

		Entity firstCreated = static_cast<Entity>(registry.getEntityCount());

		Entity lastCreated = static_cast<Entity>(registry.getEntityCount());

		for (Entity e = 0; e < (lastCreated - firstCreated); e++)
		{
			auto& t = registry.getComponent<Transform>(firstCreated + e);
			t.position += vec3(-10.0f, 0.0f, 0.0f);
		}

		Entity leftFootEntity = tags["foot_left"];
		registry.addComponent<Foot>(leftFootEntity);

		Entity rightFootEntity = tags["foot_right"];
		registry.addComponent<Foot>(rightFootEntity);

		auto& groundMat = services.materials2D().createMaterial("ground");
		groundMat.color = ColorPalette::LightGreen;

		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 0.0f},
												  {Primitives::Box::POS_Y, -24.0f},
												  {Primitives::Box::SIZE_X, 200.0f},
												  {Primitives::Box::SIZE_Y, 20.0f}},
									.material = groundMat,
									.combination = CombinationType::Addition});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;

		Entity globalSettingsEnt = registry.createEntity();
		auto& settings = registry.addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = -10.0f;
		registry.setComponentDirty(settings);
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		float delta = services.time().deltaTime();

		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		updatePhysics(delta, registry);
	}

	int m_currentFoot = 0;
	bool m_feetTouching = false;
	void updatePhysics(float delta, Registry& registry)
	{
		auto componentArray = registry.getComponentArray<Foot>();
		auto rigidBodies = registry.getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			auto& foot = componentArray->getDataAtIdx(i);
			auto& rb = rigidBodies->getDataFromEntity(componentArray->getEntityAtIdx(i));

			if (i != m_currentFoot)
			{
				if (foot.onFloor)
				{
					rb.isFixed = true;
					registry.setComponentDirty(rb);
				}
				continue;
			}

			auto& headRB = registry.getComponent<RigidBody2D>(m_head);
			headRB.pendingImpulseForce += vec2(0.0f, 1.0f);

			rb.isFixed = false;
			registry.setComponentDirty(rb);

			// Start step
			if (!foot.stepStarted)
			{
				if (foot.onFloor)
				{
					foot.initialPos =
						vec2(registry.getComponent<Transform>(componentArray->getEntityAtIdx(i)).position);
					foot.stepStarted = true;
					foot.t = 0.0f;
					// rb.position = foot.initialPos + vec2(0.0f, 0.1f);
					rb.isFixed = false;
					registry.setComponentDirty(rb);
				}
			}
			else
			{
				// End step
				if (foot.onFloor && foot.t > 0.1f)
				{
					foot.stepStarted = false;
					m_currentFoot = (m_currentFoot + 1) % componentArray->getSize();
					// WeirdEngine::Logger::log("Switching foot: " + std::to_string(m_currentFoot));
				}
				else
				{
					vec2 f;

					if (foot.t < 0.1f)
					{
						f = (foot.direction + vec2(0.0f, 1.0f)) * foot.forceMagnitude;
					}
					else if (foot.t < 0.25f)
					{
						f = (foot.direction + vec2(0.0f, 0.0f)) * foot.forceMagnitude;
					}
					else
					{
						f = (foot.direction + vec2(0.0f, -10.0f * foot.t)) * foot.forceMagnitude * foot.t;
					}

					if (m_feetTouching)
						f.x = 0.0f;

					rb.pendingImpulseForce += f;

					// vec2 offset = vec2(-std::sin(foot.t), 1.0f - std::abs(std::cos(2.0f * foot.t)));
					// offset.y *= 0.5f;
					// getSimulation().setPosition(rb.simulationId, foot.initialPos + offset);
					foot.t += 0.5f * delta;
				}
			}
		}

		m_feetTouching = false;
	}

	void onEntityCollision(Registry& registry, ServiceProvider& services,
						   WeirdEngine::EntityCollisionEvent& event) override
	{
		Entity entityA = event.entityA;
		Entity entityB = event.entityB;

		if (registry.hasComponent<Foot>(entityA) && registry.hasComponent<Foot>(entityB))
		{
			m_feetTouching = true;
		}
	}

	void onEntityShapeCollision(Registry& registry, ServiceProvider& services,
								WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		Entity entity = event.entity;
		if (registry.hasComponent<Foot>(entity))
		{
			auto& foot = registry.getComponent<Foot>(entity);
			if (event.raw.state == CollisionState::START)
			{
				foot.onFloor = true;
			}
			else if (event.raw.state == CollisionState::END)
			{

				foot.onFloor = false;
			}
		}
	}
};
