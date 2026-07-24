#pragma once

#include <weird-engine.h>

#include "globals.h"
#include "weird-renderer/core/Display.h"

using namespace WeirdEngine;

class TextScene : public Scene2D
{
public:
	TextScene() {};

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

	void onStart(ECSManager& ecs) override
	{
		m_debugInput = true;
		m_debugFly = true;

		{
			float vars[8] = {15.0f, -50.0f, 250.0f, 50.0f};
			addShape(DefaultShapes::BOX, vars, DisplaySettings::LightGray, CombinationType::SmoothAddition);
		}

		ecs.getComponent<Transform>(m_mainCamera).position = g_cameraPositon;
		m_lastResolutionHash = Display::width + Display::height;

		{
			m_worldText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_worldText);
			t.position = vec3(15.0f, 12.0f, 0.0f);

			auto& text = ecs.addComponent<TextRenderer>(m_worldText);
			text.text = "WORLD TEXT";
			text.material = DisplaySettings::Cyan;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		{
			m_worldMouseText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_worldMouseText);
			t.position = vec3(0.0f, 0.0f, 0.0f);

			auto& text = ecs.addComponent<TextRenderer>(m_worldMouseText);
			text.text = "WORLD MOUSE";
			text.material = DisplaySettings::LightBlue;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		{
			m_counterText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_counterText);
			t.position = vec3(static_cast<float>(Display::width) * 0.5f, 50.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(m_counterText);
			text.text = "0";
			text.material = DisplaySettings::LightGreen;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_centerText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_centerText);
			t.position = vec3(static_cast<float>(Display::width) * 0.5f, 20.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(m_centerText);
			text.text = "CENTERED";
			text.material = DisplaySettings::Yellow;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_leftText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_leftText);
			t.position = vec3(10.0f, 20.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(m_leftText);
			text.text = "LEFT";
			text.material = DisplaySettings::Orange;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_rightText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_rightText);
			t.position = vec3(static_cast<float>(Display::width) - 10.0f, 20.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(m_rightText);
			text.text = "RIGHT";
			text.material = DisplaySettings::Magenta;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}

		{
			m_nonResponsiveText = ecs.createEntity();
			auto& t = ecs.addComponent<Transform>(m_nonResponsiveText);
			t.position =
				vec3(static_cast<float>(Display::width) - 10.0f, static_cast<float>(Display::height) - 10.0f, 0.0f);

			auto& text = ecs.addComponent<UITextRenderer>(m_nonResponsiveText);
			text.text = "STUCK";
			text.material = DisplaySettings::Red;
			text.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			text.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}
	}

	void onUpdate(float delta, ECSManager& ecs) override
	{
		if (Input::GetKeyDown(Input::Q) || Input::GetGamepadButtonDown(Input::GamepadButton::North))
		{
			setSceneComplete();
		}

		m_counter++;
		{
			auto& text = ecs.getComponent<UITextRenderer>(m_counterText);
			text.text = std::to_string(m_counter);
			ecs.setComponentDirty(text);

			auto& t = ecs.getComponent<Transform>(m_counterText);
			t.position.x = Input::GetMouseX() + 20.0f;
			t.position.y = Input::GetMouseY() + 10.0f;
		}

		{
			auto& cameraTransform = ecs.getComponent<Transform>(m_mainCamera);
			vec2 mouseScreen = vec2(Input::GetMouseX() + 20.0f, Input::GetMouseY() - 10.0f);
			vec2 mouseWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, mouseScreen);

			auto& t = ecs.getComponent<Transform>(m_worldMouseText);
			t.position.x = mouseWorld.x;
			t.position.y = mouseWorld.y;
			ecs.setComponentDirty(t);
		}

		int hash = Display::width + Display::height;
		if (hash != m_lastResolutionHash)
		{
			m_lastResolutionHash = hash;

			float halfW = static_cast<float>(Display::width) * 0.5f;

			ecs.getComponent<Transform>(m_counterText).position = vec3(halfW, 20.0f, 0.0f);
			ecs.getComponent<Transform>(m_centerText).position = vec3(halfW, 40.0f, 0.0f);
			ecs.getComponent<Transform>(m_rightText).position =
				vec3(static_cast<float>(Display::width) - 10.0f, 20.0f, 0.0f);
		}
	}
};