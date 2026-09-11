#pragma once

#include <random>

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

#include "globals.h"

using namespace WeirdEngine;
// Example scene demonstrating how to create a rope of connected circles using springs.
class ShapeCombinatiosScene : public Scene2D
{
public:
	ShapeCombinatiosScene() {}

	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		// Local coordinate p centered at (0, 0)
		Vec2Expr p = point();

		// Shape parameters (scaled 10x for UI):
		// var(0): Outer radius (default 30.0f)
		// var(1): Spike amplitude (default 5.0f)
		// var(2): Star points count / spokes (default 8.0f)
		// var(3): Angular rotation speed (default 1.5f)
		Expr star = sdStar(p, Expr(var(0)), Expr(var(1)), Expr(var(2)), Expr(var(3)));

		auto song = WeirdAudio::SdfSong::create("star_song", star);

		// Default parameter values for CPU audio evaluation and shader
		song->setParameter(0, 30.0f); // Outer radius
		song->setParameter(1, 5.0f);  // Spike amplitude
		song->setParameter(2, 8.0f);  // Star points (4/4 groove)
		song->setParameter(3, 0.5f);  // Angular rotation speed

		return song;
	}

private:
	Entity m_circle = INVALID_ENTITY;
	float m_circleRadious = 0.0f;
	vec2 m_initialMousePositionInWorld;

	std::vector<Entity> m_uiPoints;

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize SDF procedural music with the shape-driven star song and directly create its UI visualization
		// shape
		auto shapesSong = createSceneSong();

		auto& songMat = services.materials2D().createMaterial("song_score");
		songMat.color = vec4(ColorPalette::White, 0.25f);

		Entity soundVisualization = services.audio().setSong(shapesSong, {.material = songMat});

		auto& floorMat = services.materials2D().createMaterial("floor");
		floorMat.color = ColorPalette::Gray;

		auto& circleMat = services.materials2D().createMaterial("circle");
		circleMat.color = vec4(ColorPalette::Yellow, 0.25f);

		auto& voidMat = services.materials2D().createMaterial("void");
		voidMat.color = ColorPalette::Black;

		// Floor shape
		services.shapes().addShape({.shapeId = DefaultShapes::SINE,
									.variables = {{Primitives::SineWave::AMPLITUDE, 0.5f},
												  {Primitives::SineWave::PERIOD, 2.5f},
												  {Primitives::SineWave::SPEED, 1.0f}},
									.material = floorMat,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 0});

		std::random_device rd;
		std::mt19937 gen(rd());
		float range = 20.0f;
		std::uniform_real_distribution<float> distrib(-range, range);

		// Circle
		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 7.5f},
												  {Primitives::Circle::RADIUS, 5.0f}},
									.material = circleMat,
									.combination = CombinationType::Addition,
									.hasCollision = true,
									.group = 2});

		// Subtract star
		services.shapes().addShape({.shapeId = DefaultShapes::STAR,
									.variables = {-2.5f + 15.0f, 12.5f, 5.0f, 0.5f, 13.0f, 5.0f},
									.material = voidMat,
									.combination = CombinationType::SmoothSubtraction,
									.hasCollision = true,
									.group = 2});

		// Cursor circle
		m_circle = services.shapes().addShape(
			{.shapeId = DefaultShapes::CIRCLE,
			 .variables = {{Primitives::Circle::POS_X, 250.0f}, {Primitives::Circle::POS_Y, 10.0f}},
			 .material = voidMat,
			 .combination = CombinationType::Subtraction,
			 .hasCollision = true,
			 .group = CustomShape::GLOBAL_GROUP});

		services.shapes().addShape({.shapeId = DefaultShapes::CIRCLE,
									.variables = {{Primitives::Circle::POS_X, 15.0f},
												  {Primitives::Circle::POS_Y, 0.0f},
												  {Primitives::Circle::RADIUS, 30.0f}},
									.material = voidMat,
									.combination = CombinationType::Intersection,
									.hasCollision = true,
									.group = CustomShape::GLOBAL_GROUP});

		std::vector<Material2DHandle> uiMats;
		for (int i = 0; i < 10; ++i)
		{
			auto& m = services.materials2D().createMaterial("ui_" + std::to_string(i));
			m.color = ColorPalette::Default[(4 + i) % ColorPalette::Default.size()];
			uiMats.push_back(m.id);
		}

		for (int i = 0; i < 10; ++i)
		{
			auto ee = registry.createEntity();
			auto& t = registry.addComponent<Transform>(ee);
			t.position = vec3(15.0f, 15.0f, 10.0f);

			auto& ui = registry.addComponent<UIDot>(ee);
			ui.materialId = uiMats[i % uiMats.size()].id;

			m_uiPoints.push_back(ee);
		}

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		float delta = services.time().deltaTime();
		g_cameraPositon = registry.getComponent<Transform>(services.render().getCameraEntity()).position;

		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
		float x = services.input().getMouseX();
		float y = services.input().getMouseY();

		// Transform mouse coordinates to world space
		vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		if (services.input().getMouseButtonDown(Input::RightClick))
		{
			m_initialMousePositionInWorld = mousePositionInWorld;
		}
		else if (services.input().getGamepadButtonDown(Input::GamepadButton::LeftShoulder))
		{
			float halfWidth = Display::width / 2.0f;
			float halfHeight = Display::height / 2.0f;

			m_initialMousePositionInWorld =
				ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(halfWidth, halfHeight));
		}

		if (services.input().getMouseButton(Input::RightClick))
		{
			vec2 v = mousePositionInWorld - m_initialMousePositionInWorld;
			m_circleRadious = (std::min)(10.0f, length(v));
		}
		else if (services.input().getGamepadButton(Input::GamepadButton::LeftShoulder))
		{
			m_circleRadious = (std::min)(10.0f, m_circleRadious + (10.0f * delta));
		}
		else
		{
			m_circleRadious -= delta * 10.0f * (m_circleRadious + 1.0f);
			m_circleRadious = (std::max)(0.0f, m_circleRadious);
		}

		{
			CustomShape& cs = registry.getComponent<CustomShape>(m_circle);
			cs.parameters[0] = m_initialMousePositionInWorld.x;
			cs.parameters[1] = m_circleRadious <= 0.0f ? -1000.0f : m_initialMousePositionInWorld.y;
			cs.parameters[2] = m_circleRadious;

			registry.setComponentDirty(cs);
		}

		services.audio().setTension(m_circleRadious / 10.0f);

		float volume = WeirdAudio::AudioEngine::getInstance().getAudioData().currentVolume;
		glm::vec2 center = glm::vec2(75.0f, 75.0f); // Screen center X, Y
		float radius = 50.0f - (volume * 50.0f);	// Distance from center
		float speed = 1.0f;							// How fast they rotate
		float spacing =
			2.0f * 3.14f / (static_cast<float>(m_uiPoints.size())); // Gap between each dot along the circle arc

		for (int i = 0; i < m_uiPoints.size(); i++)
		{
			// Calculate angle: Time moves them, 'i' spreads them out
			float angle = (services.time().time() * speed) + (i * spacing);

			float x = center.x + std::cos(angle) * radius;
			float y = center.y + std::sin(angle) * radius;

			auto& t = registry.getComponent<Transform>(m_uiPoints[i]);
			t.position = vec3(x, y, 0.0f);
		}
	}
};
