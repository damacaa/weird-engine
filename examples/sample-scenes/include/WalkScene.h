#pragma once

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

private:
	Entity m_head;

	// Inherited via Scene
	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		m_background.type = BackgroundType::Sky;
		m_background.primaryColor = vec4(0.2f, 0.55f, 0.9f, 1.0f);
		m_background.secondaryColor = vec4(0.4f, 0.75f, 0.85f, 1.0f);
		m_background.scale = 0.2f;

		auto tags = loadWeirdFile(ASSETS_PATH "man.weird");

		Entity firstCreated = static_cast<Entity>(ecs.getEntityCount());

		Entity lastCreated = static_cast<Entity>(ecs.getEntityCount());

		for (Entity e = 0; e < (lastCreated - firstCreated); e++)
		{
			auto& t = ecs.getComponent<Transform>(firstCreated + e);
			t.position += vec3(-10.0f, 0.0f, 0.0f);
		}

		Entity leftFootEntity = tags["foot_left"];
		ecs.addComponent<Foot>(leftFootEntity);

		Entity rightFootEntity = tags["foot_right"];
		ecs.addComponent<Foot>(rightFootEntity);

		m_head = tags["head"];

		float boundsVars2[8]{0.0f, -24.0f, 200.0f, 20.0f};
		Entity inside =
			addShape(DefaultShapes::BOX, boundsVars2, DisplaySettings::LightGreen, CombinationType::Addition);

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;

		Entity globalSettingsEnt = ecs.createEntity();
		auto& settings = ecs.addComponent<GlobalPhysicsSettings>(globalSettingsEnt);
		settings.gravity = -10.0f;
		ecs.setComponentDirty(settings);
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		g_cameraPositon = ecs.getComponent<Transform>(m_mainCamera).position;

		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}

		updatePhysics(delta, ecs);
	}

	int m_currentFoot = 0;
	bool m_feetTouching = false;
	void updatePhysics(float delta, ECSManager& ecs)
	{
		auto componentArray = ecs.getComponentArray<Foot>();
		auto rigidBodies = ecs.getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			auto& foot = componentArray->getDataAtIdx(i);
			auto& rb = rigidBodies->getDataFromEntity(componentArray->getEntityAtIdx(i));

			if (i != m_currentFoot)
			{
				if (foot.onFloor)
				{
					rb.isFixed = true;
					ecs.setComponentDirty(rb);
				}
				continue;
			}

			auto& headRB = ecs.getComponent<RigidBody2D>(m_head);
			headRB.pendingImpulseForce += vec2(0.0f, 1.0f);

			rb.isFixed = false;
			ecs.setComponentDirty(rb);

			// Start step
			if (!foot.stepStarted)
			{
				if (foot.onFloor)
				{
					foot.initialPos = vec2(ecs.getComponent<Transform>(componentArray->getEntityAtIdx(i)).position);
					foot.stepStarted = true;
					foot.t = 0.0f;
					// rb.position = foot.initialPos + vec2(0.0f, 0.1f);
					rb.isFixed = false;
					ecs.setComponentDirty(rb);
				}
			}
			else
			{
				// End step
				if (foot.onFloor && foot.t > 0.1f)
				{
					foot.stepStarted = false;
					m_currentFoot = (m_currentFoot + 1) % componentArray->getSize();
					// WeirdEngine::Logger::Log("Switching foot: " + std::to_string(m_currentFoot));
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

	void onEntityCollision(ECSManager& ecs, WeirdEngine::EntityCollisionEvent& event) override
	{
		Entity entityA = event.entityA;
		Entity entityB = event.entityB;

		if (ecs.hasComponent<Foot>(entityA) && ecs.hasComponent<Foot>(entityB))
		{
			m_feetTouching = true;
		}
	}

	void onEntityShapeCollision(ECSManager& ecs, WeirdEngine::EntityShapeCollisionEvent& event) override
	{
		Entity entity = event.entity;
		if (ecs.hasComponent<Foot>(entity))
		{
			auto& foot = ecs.getComponent<Foot>(entity);
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
