#pragma once

#include "weird-audio/SdfSong.h"
#include <weird-engine.h>

#include "globals.h"
#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;

class TextScene : public Scene2D
{
public:
	TextScene() {};

	static std::shared_ptr<WeirdAudio::SdfSong> createSceneSong()
	{
		using namespace SDF;
		Vec2Expr p = SDF::point();

		Expr vBar = sdBox(p, Vec2Expr(4.0f, 25.0f));
		Expr hBar = sdBox(translate(p, Vec2Expr(0.0f, 22.0f)), Vec2Expr(18.0f, 6.0f));

		Expr textShape = sdfUnion(vBar, hBar);

		return WeirdAudio::SdfSong::create("text", textShape);
	}

private:
	Entity m_counterText = INVALID_ENTITY;
	Entity m_centerText = INVALID_ENTITY;
	Entity m_leftText = INVALID_ENTITY;
	Entity m_rightText = INVALID_ENTITY;
	Entity m_worldText = INVALID_ENTITY;
	Entity m_worldMouseText = INVALID_ENTITY;
	Entity m_nonResponsiveText = INVALID_ENTITY;

	int m_counter = 0;
	int m_lastResolutionHash = 0;

	void onStart(Registry& registry, ServiceProvider& services) override
	{
		services.debug().setDebugInput(true);
		services.debug().setDebugFly(true);

		// Initialize audio with scene-defined text song
		services.audio().setSong(createSceneSong(), {.mode = SongVisualizationMode::UI});

		auto& floorMat = services.materials2D().createMaterial("floor");
		floorMat.color = ColorPalette::LightGray;

		auto& cyanMat = services.materials2D().createMaterial("text_cyan");
		cyanMat.color = ColorPalette::Cyan;

		auto& lightBlueMat = services.materials2D().createMaterial("text_blue");
		lightBlueMat.color = ColorPalette::LightBlue;

		auto& greenMat = services.materials2D().createMaterial("text_green");
		greenMat.color = ColorPalette::LightGreen;

		auto& yellowMat = services.materials2D().createMaterial("text_yellow");
		yellowMat.color = ColorPalette::Yellow;

		auto& orangeMat = services.materials2D().createMaterial("text_orange");
		orangeMat.color = ColorPalette::Orange;

		auto& magentaMat = services.materials2D().createMaterial("text_magenta");
		magentaMat.color = ColorPalette::Magenta;

		auto& redMat = services.materials2D().createMaterial("text_red");
		redMat.color = ColorPalette::Red;

		services.shapes().addShape({.shapeId = DefaultShapes::BOX,
									.variables = {{Primitives::Box::POS_X, 15.0f},
												  {Primitives::Box::POS_Y, -50.0f},
												  {Primitives::Box::SIZE_X, 250.0f},
												  {Primitives::Box::SIZE_Y, 50.0f}},
									.material = floorMat,
									.combination = CombinationType::SmoothAddition});

		registry.getComponent<Transform>(services.render().getCameraEntity()).position = g_cameraPositon;
		m_lastResolutionHash = Display::width + Display::height;

		{
			m_worldText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_worldText);
			t.position = vec3(15.0f, 12.0f, 0.0f);

			auto& text = registry.addComponent<TextRenderer>(m_worldText);
			text.text = "WORLD TEXT";
			text.material = cyanMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		{
			m_worldMouseText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_worldMouseText);
			t.position = vec3(0.0f, 0.0f, 0.0f);

			auto& text = registry.addComponent<TextRenderer>(m_worldMouseText);
			text.text = "WORLD MOUSE";
			text.material = lightBlueMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		{
			m_counterText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_counterText);
			t.position = vec3(static_cast<float>(Display::width) * 0.5f, 50.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(m_counterText);
			text.text = "0";
			text.material = greenMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_centerText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_centerText);
			t.position = vec3(static_cast<float>(Display::width) * 0.5f, 20.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(m_centerText);
			text.text = "CENTERED";
			text.material = yellowMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_leftText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_leftText);
			t.position = vec3(10.0f, 20.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(m_leftText);
			text.text = "LEFT";
			text.material = orangeMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_rightText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_rightText);
			t.position = vec3(static_cast<float>(Display::width) - 10.0f, 20.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(m_rightText);
			text.text = "RIGHT";
			text.material = magentaMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_nonResponsiveText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(m_nonResponsiveText);
			t.position =
				vec3(static_cast<float>(Display::width) - 10.0f, static_cast<float>(Display::height) - 10.0f, 0.0f);

			auto& text = registry.addComponent<UITextRenderer>(m_nonResponsiveText);
			text.text = "STUCK";
			text.material = redMat.id;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}
	}

	void onUpdate(Registry& registry, ServiceProvider& services) override
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}

		m_counter++;
		{
			auto& text = registry.getComponent<UITextRenderer>(m_counterText);
			text.text = std::to_string(m_counter);
			registry.setComponentDirty(text);

			auto& t = registry.getComponent<Transform>(m_counterText);
			t.position.x = services.input().getMouseX() + 20.0f;
			t.position.y = services.input().getMouseY() + 10.0f;
		}

		{
			auto& cameraTransform = registry.getComponent<Transform>(services.render().getCameraEntity());
			vec2 mouseScreen = vec2(services.input().getMouseX() + 20.0f, services.input().getMouseY() - 10.0f);
			vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, mouseScreen);

			auto& t = registry.getComponent<Transform>(m_worldMouseText);
			t.position.x = mouseWorld.x;
			t.position.y = mouseWorld.y;
			registry.setComponentDirty(t);
		}

		int hash = Display::width + Display::height;
		if (hash != m_lastResolutionHash)
		{
			m_lastResolutionHash = hash;

			float halfW = static_cast<float>(Display::width) * 0.5f;

			registry.getComponent<Transform>(m_counterText).position = vec3(halfW, 20.0f, 0.0f);
			registry.getComponent<Transform>(m_centerText).position = vec3(halfW, 40.0f, 0.0f);
			registry.getComponent<Transform>(m_rightText).position =
				vec3(static_cast<float>(Display::width) - 10.0f, 20.0f, 0.0f);
		}
	}
};