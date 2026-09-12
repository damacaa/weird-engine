#pragma once

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
class CollisionHandlingScene : public Scene2D
{
public:
	CollisionHandlingScene() {};

	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		Vec2Expr p = SDF::point();

		// Sharp angular collision geometry: box combined with triangle (scaled 10x for UI)
		Expr box = sdBox(p, Vec2Expr(20.0f, 12.0f));
		Expr tri = sdTriangle(p, 25.0f, 20.0f);
		Expr colShape = sdfUnion(box, tri);

		return WeirdAudio::SdfSong::create("collision-handling", colShape);
	}

private:
	// Inherited via Scene
	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio with scene-defined collision handling song
		services.audio().setSong(createSceneSong(), {.mode = SongVisualizationMode::UI});

		auto& floorMat = services.materials2D().createMaterial("floor");
		floorMat.color = ColorPalette::LightGray;

		std::vector<Material2DHandle> ballMats;
		for (int i = 0; i < 8; ++i)
		{
			auto& mat = services.materials2D().createMaterial("ball_" + std::to_string(i));
			mat.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			ballMats.push_back(mat.id);
		}

		for (size_t i = 0; i < 10; i++)
		{
			float y = 10.0f + static_cast<float>(i);
			float x = 2.0f * static_cast<float>(i);

			float z = 0;

			Entity entity = registry.createEntity();
			Transform& t = registry.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			Dot& dot = registry.addComponent<Dot>(entity);
			dot.materialId = ballMats[i % ballMats.size()].id;

			RigidBody2D& rb = registry.addComponent<RigidBody2D>(entity);
		}

		// Floor
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 5.0f},
												  {Primitives::Circle::RADIUS, 25.0f}},
									.material = floorMat});

		auto floor = services.shapes().addShape({.shapeId = DefaultShapes::BOX,
												 .variables = {{Primitives::Box::POS_X, 15.0f},
															   {Primitives::Box::POS_Y, -50.0f},
															   {Primitives::Box::SIZE_X, 250.0f},
															   {Primitives::Box::SIZE_Y, 50.0f}},
												 .material = floorMat,
												 .combination = CombinationType::SmoothAddition});
		registry.getComponent<CustomShape>(floor).smoothFactor = 3.0f;

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 5.0f},
												  {Primitives::Circle::RADIUS, 20.0f}},
									.material = floorMat,
									.combination = CombinationType::Subtraction});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	float m_currentTime = 0.0f;
	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		m_currentTime = services.time().time();
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	float m_lastTime = 0.0f;
	void onPhysicsRigidBodyCollision(Simulation2D& simulation, WeirdEngine::PhysicsCollisionEvent& event) override
	{
		float t = m_currentTime;
		if (t - m_lastTime < 0.1f)
			return; // Avoid multiple collisions in a short time

		m_lastTime = t;
		// simulation.setPosition(event.bodyA, vec2(15.0f, 15.0f));
		// simulation.addImpulseForce(event.bodyA, vec2(2.0f * sinf(t), -20.0f));
		simulation.setPosition(event.bodyB, vec2(15.0f, 24.45f));
		simulation.addImpulseForce(event.bodyB, vec2(20.0f * sinf(10.0f * t), -30.0f));

		// Entity a = registry.getComponentArray<RigidBody2D>()->getDataAtIdx(event.bodyA).Owner;
		// Transform &at = registry.getComponent<Transform>(a);
		// at.position.x = 15.0f + sinf(event.bodyB * 123.4565f + t);
		// at.position.y = 5.0f + (event.bodyA % 10) * 2.5f;
		// at.isDirty = true;
	}
};
