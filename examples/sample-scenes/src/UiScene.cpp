#include "UiScene.h"

#include "globals.h"
#include <cstdio>
#include <string>

using namespace WeirdEngine;

namespace UiSceneNamespace
{
	State& getState(Registry& registry)
	{
		State* state = registry.getState<State>();
		WEIRD_ASSERT(state != nullptr, "UiScene State is missing: stateInitSystem must run first");
		return *state;
	}

	void stateInitSystem(Registry& registry, ServiceProvider& services)
	{
		registry.emplaceState<State>();
	}

	void setupEnvironmentSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		services.debug().setDebugInput(true);
		services.debug().setDebugFly(false);

		state.lastResolutionHash = Display::width + Display::height;
	}

	void setupUiMaterialsSystem(Registry& registry, ServiceProvider& services)
	{
		Material2D& cyanMat = services.materials2D().createMaterial("ui_cyan");
		cyanMat.color = ColorPalette::Cyan;

		Material2D& lightBlueMat = services.materials2D().createMaterial("ui_blue");
		lightBlueMat.color = ColorPalette::LightBlue;

		Material2D& greenMat = services.materials2D().createMaterial("ui_green");
		greenMat.color = ColorPalette::LightGreen;

		Material2D& yellowMat = services.materials2D().createMaterial("ui_yellow");
		yellowMat.color = ColorPalette::Yellow;

		Material2D& orangeMat = services.materials2D().createMaterial("ui_orange");
		orangeMat.color = ColorPalette::Orange;

		Material2D& magentaMat = services.materials2D().createMaterial("ui_magenta");
		magentaMat.color = ColorPalette::Magenta;

		Material2D& btnMat = services.materials2D().createMaterial("ui_btn_body");
		btnMat.color = vec4(ColorPalette::LightGray, 0.9f);

		Material2D& toggleMat = services.materials2D().createMaterial("ui_toggle_body");
		toggleMat.color = vec4(ColorPalette::LightBlue, 0.85f);
	}

	void setupButtonsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& btnMat = services.materials2D().get("ui_btn_body");
		auto& toggleMat = services.materials2D().get("ui_toggle_body");

		// Interactive Counter Click Button (Box shape + ShapeButton)
		{
			state.clickButton = services.shapes().addUIShape({.shapeId = DefaultShapes::BOX,
															  .variables = {{Primitives::Box::POS_X, 120.0f},
																			{Primitives::Box::POS_Y, 150.0f},
																			{Primitives::Box::SIZE_X, 90.0f},
																			{Primitives::Box::SIZE_Y, 22.0f}},
															  .material = btnMat});
			auto& btn = registry.addComponent<ShapeButton>(state.clickButton);
			btn.clickPadding = 8.0f;
		}

		// Reset Counter Button
		{
			state.resetButton = services.shapes().addUIShape({.shapeId = DefaultShapes::BOX,
															  .variables = {{Primitives::Box::POS_X, 120.0f},
																			{Primitives::Box::POS_Y, 210.0f},
																			{Primitives::Box::SIZE_X, 90.0f},
																			{Primitives::Box::SIZE_Y, 22.0f}},
															  .material = btnMat});
			auto& btn = registry.addComponent<ShapeButton>(state.resetButton);
			btn.clickPadding = 8.0f;
		}

		// Toggle Button (ShapeToggle)
		{
			state.modeToggle = services.shapes().addUIShape({.shapeId = DefaultShapes::BOX,
															 .variables = {{Primitives::Box::POS_X, 120.0f},
																		   {Primitives::Box::POS_Y, 270.0f},
																		   {Primitives::Box::SIZE_X, 90.0f},
																		   {Primitives::Box::SIZE_Y, 22.0f}},
															 .material = toggleMat});
			auto& toggle = registry.addComponent<ShapeToggle>(state.modeToggle);
			toggle.active = false;
			toggle.clickPadding = 8.0f;
		}
	}

	void setupTextLabelsSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		auto& cyanMat = services.materials2D().get("ui_cyan");
		auto& lightBlueMat = services.materials2D().get("ui_blue");
		auto& greenMat = services.materials2D().get("ui_green");
		auto& yellowMat = services.materials2D().get("ui_yellow");
		auto& orangeMat = services.materials2D().get("ui_orange");
		auto& magentaMat = services.materials2D().get("ui_magenta");

		// 1. Title
		{
			state.titleText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.titleText);
			t.position = vec3(20.0f, 20.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.titleText);
			tr.text = "UI & INTERACTIVE CONTROLS SHOWCASE";
			tr.material = cyanMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		// 2. Center Top Anchor Text
		{
			state.centerText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.centerText);
			t.position = vec3(static_cast<float>(Display::width) * 0.5f, 20.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.centerText);
			tr.text = "[CENTER ANCHOR]";
			tr.material = yellowMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		// 3. Right Top Anchor Text
		{
			state.rightText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.rightText);
			t.position = vec3(static_cast<float>(Display::width) - 20.0f, 20.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.rightText);
			tr.text = "[RIGHT ANCHOR]";
			tr.material = magentaMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		// 4. Counter Label Text
		{
			state.counterText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.counterText);
			t.position = vec3(240.0f, 150.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.counterText);
			tr.text = "Click Count: 0";
			tr.material = greenMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}

		// 5. Toggle Status Label Text
		{
			state.toggleLabelText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.toggleLabelText);
			t.position = vec3(240.0f, 270.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.toggleLabelText);
			tr.text = "Toggle Mode: OFF";
			tr.material = orangeMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		}

		// 6. Mouse Screen-Space Tracking Text
		{
			state.mouseScreenText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.mouseScreenText);
			t.position = vec3(0.0f, 0.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.mouseScreenText);
			tr.text = "Mouse Screen: (0, 0)";
			tr.material = lightBlueMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		}

		// 7. Hint text at bottom
		{
			state.hintText = registry.createEntity();
			auto& t = registry.addComponent<Transform>(state.hintText);
			t.position = vec3(20.0f, static_cast<float>(Display::height) - 20.0f, 0.0f);

			auto& tr = registry.addComponent<UITextRenderer>(state.hintText);
			tr.text = "Click the UI buttons to interact | [Q]: Next Scene";
			tr.material = cyanMat.id;
			tr.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
			tr.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		}
	}

	void sceneControlSystem(Registry& registry, ServiceProvider& services)
	{
		if (services.input().getKeyDown(Input::Q) || services.input().getGamepadButtonDown(Input::GamepadButton::North))
		{
			services.sceneControl().goToNextScene();
		}
	}

	void buttonInteractionSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		// Check Click Button
		if (registry.isEntityValid(state.clickButton) && registry.hasComponent<ShapeButton>(state.clickButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.clickButton);
			if (btn.state == ButtonState::Down)
			{
				state.clickCount++;
				services.audio().playSound({0.03f, 700.0f, false, vec3(0.0f), 1});
			}
		}

		// Check Reset Button
		if (registry.isEntityValid(state.resetButton) && registry.hasComponent<ShapeButton>(state.resetButton))
		{
			auto& btn = registry.getComponent<ShapeButton>(state.resetButton);
			if (btn.state == ButtonState::Down)
			{
				state.clickCount = 0;
				services.audio().playSound({0.03f, 400.0f, false, vec3(0.0f), 1});
			}
		}

		// Check Toggle Button
		if (registry.isEntityValid(state.modeToggle) && registry.hasComponent<ShapeToggle>(state.modeToggle))
		{
			auto& toggle = registry.getComponent<ShapeToggle>(state.modeToggle);
			if (toggle.state == ButtonState::Down)
			{
				state.darkMode = toggle.active;
				services.audio().playSound({0.04f, toggle.active ? 880.0f : 550.0f, false, vec3(0.0f), 1});
			}
		}
	}

	void labelUpdateSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		char buf[64];

		// Update Counter Text
		if (registry.isEntityValid(state.counterText) && registry.hasComponent<UITextRenderer>(state.counterText))
		{
			auto& tr = registry.getComponent<UITextRenderer>(state.counterText);
			std::snprintf(buf, sizeof(buf), "Click Count: %d", state.clickCount);
			tr.text = buf;
			registry.setComponentDirty(tr);
		}

		// Update Toggle Label Text
		if (registry.isEntityValid(state.toggleLabelText) &&
			registry.hasComponent<UITextRenderer>(state.toggleLabelText))
		{
			auto& tr = registry.getComponent<UITextRenderer>(state.toggleLabelText);
			std::snprintf(buf, sizeof(buf), "Toggle Mode: %s", state.darkMode ? "ON (Active)" : "OFF");
			tr.text = buf;
			registry.setComponentDirty(tr);
		}
	}

	void mouseTrackingSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);
		float mx = services.input().getMouseX();
		float my = services.input().getMouseY();

		if (registry.isEntityValid(state.mouseScreenText) && registry.hasComponent<Transform>(state.mouseScreenText))
		{
			auto& t = registry.getComponent<Transform>(state.mouseScreenText);
			t.position.x = mx + 20.0f;
			t.position.y = my + 10.0f;
			registry.setComponentDirty(t);

			char buf[64];
			auto& tr = registry.getComponent<UITextRenderer>(state.mouseScreenText);
			std::snprintf(buf, sizeof(buf), "Screen: (%.0f, %.0f)", mx, my);
			tr.text = buf;
			registry.setComponentDirty(tr);
		}
	}

	void responsiveLayoutSystem(Registry& registry, ServiceProvider& services)
	{
		State& state = getState(registry);

		int hash = Display::width + Display::height;
		if (hash != state.lastResolutionHash)
		{
			state.lastResolutionHash = hash;
			float halfW = static_cast<float>(Display::width) * 0.5f;

			if (registry.isEntityValid(state.centerText))
			{
				auto& t = registry.getComponent<Transform>(state.centerText);
				t.position.x = halfW;
				registry.setComponentDirty(t);
			}

			if (registry.isEntityValid(state.rightText))
			{
				auto& t = registry.getComponent<Transform>(state.rightText);
				t.position.x = static_cast<float>(Display::width) - 20.0f;
				registry.setComponentDirty(t);
			}

			if (registry.isEntityValid(state.hintText))
			{
				auto& t = registry.getComponent<Transform>(state.hintText);
				t.position.y = static_cast<float>(Display::height) - 20.0f;
				registry.setComponentDirty(t);
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
} // namespace UiSceneNamespace