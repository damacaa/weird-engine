#include "WalkScene.h"

#include "globals.h"
#include <cmath>

using namespace WeirdEngine;

namespace WalkSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "WalkScene State is missing: stateInitSystem must run first");
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

		services.physics().setGravity(-10.0f);
		services.physics().setDamping(0.05f);
	}

	void loadCharacterSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto tags = services.serialization().loadWeirdFile(services.resources().assetPath("man.weird"));

		if (tags.contains("head"))
		{
			state.head = tags["head"];
		}

		if (tags.contains("foot_left"))
		{
			Entity leftFootEntity = tags["foot_left"];
			registry.addComponent<Foot>(leftFootEntity);
		}

		if (tags.contains("foot_right"))
		{
			Entity rightFootEntity = tags["foot_right"];
			registry.addComponent<Foot>(rightFootEntity);
		}
	}

	void setupGroundSystem(Registry& registry, ServiceProvider& services)
	{
		auto& groundMat = services.materials2D().createMaterial("ground");
		groundMat.color = ColorPalette::LightGreen;

		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.0f},
												  {Primitives::SineWave::PERIOD, 0.0f},
												  {Primitives::SineWave::SPEED, 0.0f},
												  {Primitives::SineWave::OFFSET, 0.0f}},
									.material = groundMat,
									.combination = CombinationType::Addition});
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void walkingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float delta = services.time().deltaTime();

		auto componentArray = registry.getComponentArray<Foot>();
		auto rigidBodies = registry.getComponentArray<RigidBody2D>();

		for (size_t i = 0; i < componentArray->getSize(); i++)
		{
			auto& foot = componentArray->getDataAtIdx(i);
			auto& rb = rigidBodies->getDataFromEntity(componentArray->getEntityAtIdx(i));

			if (static_cast<int>(i) != state.currentFoot)
			{
				if (foot.onFloor)
				{
					rb.isFixed = true;
					registry.setComponentDirty(rb);
				}
				continue;
			}

			if (registry.isEntityValid(state.head) && registry.hasComponent<RigidBody2D>(state.head))
			{
				auto& headRB = registry.getComponent<RigidBody2D>(state.head);
				headRB.pendingImpulseForce += vec2(0.0f, 1.0f);
			}

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
					state.currentFoot = (state.currentFoot + 1) % componentArray->getSize();
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

					if (state.feetTouching)
						f.x = 0.0f;

					rb.pendingImpulseForce += f;
					foot.t += 0.5f * delta;
				}
			}
		}

		state.feetTouching = false;
	}

	void feetCollisionSystem(Registry& registry, ServiceProvider& services, EntityCollisionEvent& event)
	{
		State& state = getState(registry);
		Entity entityA = event.entityA;
		Entity entityB = event.entityB;

		if (registry.isEntityValid(entityA) && registry.isEntityValid(entityB) &&
			registry.hasComponent<Foot>(entityA) && registry.hasComponent<Foot>(entityB))
		{
			state.feetTouching = true;
		}
	}

	void floorCollisionSystem(Registry& registry, ServiceProvider& services, EntityShapeCollisionEvent& event)
	{
		Entity entity = event.entity;
		if (registry.isEntityValid(entity) && registry.hasComponent<Foot>(entity))
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

	void cameraTrackingSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		g_cameraPositon = registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position;
	}

	void cameraInitSystem(WeirdEngine::Registry& registry, WeirdEngine::ServiceProvider& services)
	{
		registry.getComponent<WeirdEngine::Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}
} // namespace WalkSceneNamespace