#include "TestHarness.h"
#include "weird-engine/Input.h"
#include "weird-engine/math/Default2DSDFs.h"
#include "weird-engine/systems/ButtonSystem.h"
#include "weird-renderer/core/Display.h"

#include <cmath>
#include <vector>

using namespace WeirdEngine;
using WeirdTests::require;

namespace
{
	void testTextButtonAndToggleComponents()
	{
		static_assert(WeirdEngine::is_snapshot_serializable_v<ShapeButton>,
					  "ShapeButton must remain snapshot-serializable.");
		static_assert(WeirdEngine::is_snapshot_serializable_v<TextButton>,
					  "TextButton must remain snapshot-serializable.");
		static_assert(WeirdEngine::is_snapshot_serializable_v<TextToggle>,
					  "TextToggle must remain snapshot-serializable.");

		WeirdRenderer::Display::width = 800;
		WeirdRenderer::Display::height = 800;

		SDFRenderSystemContext uiCtx;
		uiCtx.dotRadious = 5.0f;
		uiCtx.charSpacing = 10.0f;

		// 4 chars ("TEST"): 4 * (3 * 2 * 5) + 3 * 10 = 150px wide, 4 * 2 * 5 = 40px tall
		UITextRenderer leftBottom{};
		leftBottom.text = "TEST";
		leftBottom.horizontalAlignment = TextRenderer::HorizontalAlignment::Left;
		leftBottom.verticalAlignment = TextRenderer::VerticalAlignment::Bottom;
		Transform tf{};
		tf.position = vec3(100.0f, 200.0f, 0.0f);

		ButtonSystem::TextBounds bounds{};
		require(ButtonSystem::computeTextBounds(leftBottom, tf, uiCtx, bounds),
				"Non-empty UITextRenderer must produce valid bounds.");
		require(std::abs(bounds.minX - 100.0f) < 0.01f && std::abs(bounds.maxX - 250.0f) < 0.01f &&
					std::abs(bounds.minY - 200.0f) < 0.01f && std::abs(bounds.maxY - 240.0f) < 0.01f,
				"Left-bottom text bounds must match glyph width and height.");
		require(ButtonSystem::isPointInsideText(leftBottom, tf, uiCtx, 255.0f, 245.0f, 8.0f),
				"clickPadding must expand clickable text hit-box.");
		require(!ButtonSystem::isPointInsideText(leftBottom, tf, uiCtx, 260.0f, 220.0f, 8.0f),
				"Points outside clickPadding must not hit text.");

		// Center & Right/Top alignments
		UITextRenderer rightTop = leftBottom;
		rightTop.horizontalAlignment = TextRenderer::HorizontalAlignment::Right;
		rightTop.verticalAlignment = TextRenderer::VerticalAlignment::Top;
		require(ButtonSystem::computeTextBounds(rightTop, tf, uiCtx, bounds) &&
					std::abs(bounds.minX - (-50.0f)) < 0.01f && std::abs(bounds.maxX - 100.0f) < 0.01f &&
					std::abs(bounds.minY - 160.0f) < 0.01f && std::abs(bounds.maxY - 200.0f) < 0.01f,
				"Right-top aligned text bounds must anchor relative to top-right edge.");

		UITextRenderer centerAligned = leftBottom;
		centerAligned.horizontalAlignment = TextRenderer::HorizontalAlignment::Center;
		centerAligned.verticalAlignment = TextRenderer::VerticalAlignment::Center;
		require(ButtonSystem::computeTextBounds(centerAligned, tf, uiCtx, bounds) &&
					std::abs(bounds.minX - 25.0f) < 0.01f && std::abs(bounds.maxX - 175.0f) < 0.01f &&
					std::abs(bounds.minY - 180.0f) < 0.01f && std::abs(bounds.maxY - 220.0f) < 0.01f,
				"Center aligned text bounds must center around Transform::position.");

		// Verify momentary and toggle state transitions + UI click claiming
		Input::suppressMouseInput();
		Input::clearUIClick();
		ButtonState mState = ButtonState::Off;
		ButtonSystem::stepMomentaryButton(mState, true, true, false, true);
		require(mState == ButtonState::Off && !Input::isUIClick(),
				"Default requireClickStart must ignore hovering while mouse is already held.");
		ButtonSystem::stepMomentaryButton(mState, true, true, true, true);
		require(mState == ButtonState::Down && Input::isUIClick(),
				"Hovered press must enter Down and claim Input::isUIClick().");
		ButtonState blockedState = ButtonState::Off;
		ButtonSystem::stepMomentaryButton(blockedState, true, true, true, true);
		require(blockedState == ButtonState::Off,
				"Overlapping TextButton must respect an already-claimed Input::isUIClick().");
		Input::clearUIClick();
		ButtonSystem::stepMomentaryButton(mState, false, true, false, true);
		require(mState == ButtonState::Hold && Input::isUIClick(),
				"Continued press after leaving hover must transition Down -> Hold when requireClickStart is enabled.");
		ButtonSystem::stepMomentaryButton(mState, true, false, false, true);
		require(mState == ButtonState::Up, "Releasing mouse must transition Hold -> Up.");
		ButtonSystem::stepMomentaryButton(mState, true, false, false, true);
		require(mState == ButtonState::Off, "Subsequent frame must settle Up -> Off.");

		// Verify requireClickStart = false: hovering while held triggers, and leaving immediately releases
		Input::clearUIClick();
		ButtonState dragState = ButtonState::Off;
		const auto dragEnter = ButtonSystem::stepMomentaryButton(dragState, true, true, false, true, false);
		require(dragState == ButtonState::Down && dragEnter.enteredFromOff && Input::isUIClick(),
				"With requireClickStart disabled, hovering while mouse is held must enter Down.");
		Input::clearUIClick();
		ButtonSystem::stepMomentaryButton(dragState, true, true, false, true, false);
		require(dragState == ButtonState::Hold,
				"With requireClickStart disabled, continued hover while held must enter Hold.");
		Input::clearUIClick();
		const auto dragLeave = ButtonSystem::stepMomentaryButton(dragState, false, true, false, true, false);
		require(dragState == ButtonState::Up && !dragLeave.exitedToOff && !Input::isUIClick(),
				"With requireClickStart disabled, leaving while held must immediately transition to Up.");
		const auto dragSettle = ButtonSystem::stepMomentaryButton(dragState, false, true, false, true, false);
		require(dragState == ButtonState::Off && dragSettle.exitedToOff,
				"With requireClickStart disabled, staying outside after leaving must settle Up -> Off.");
		Input::clearUIClick();
		ButtonSystem::stepMomentaryButton(dragState, true, true, false, true, false);
		Input::clearUIClick();
		ButtonSystem::stepMomentaryButton(dragState, false, true, false, true, false);
		Input::clearUIClick();
		const auto dragReenter = ButtonSystem::stepMomentaryButton(dragState, true, true, false, true, false);
		require(dragState == ButtonState::Down && !dragReenter.enteredFromOff,
				"Re-entering on the Up frame must re-trigger Down without duplicating enteredFromOff.");

		ButtonState tState = ButtonState::Off;
		bool tActive = false;
		Input::suppressMouseInput();
		Input::clearUIClick();
		ButtonSystem::stepToggleState(tState, tActive, true, true, false);
		require(tActive && tState == ButtonState::Down, "Toggle click must activate and enter Down.");
		ButtonSystem::stepToggleState(tState, tActive, true, false, false);
		require(tActive && tState == ButtonState::Hold, "Active toggle must advance Down -> Hold.");
		ButtonSystem::stepToggleState(tState, tActive, true, true, false);
		require(!tActive && tState == ButtonState::Up, "Second click must deactivate toggle to Up.");
		ButtonSystem::stepToggleState(tState, tActive, true, false, false);
		require(!tActive && tState == ButtonState::Off, "Deactivated toggle must settle Up -> Off.");
		Input::clearUIClick();

		// Verify Registry updateTextButtons + updateTextToggles hover detection and material updates
		Registry reg;
		reg.registerComponent<Transform>();
		reg.registerComponent<UITextRenderer>();
		reg.registerComponent<TextButton>();
		reg.registerComponent<TextToggle>();

		const Entity btnEntity = reg.createEntity();
		reg.addComponent<Transform>(btnEntity).position = vec3(100.0f, 300.0f, 0.0f);
		auto& btnText = reg.addComponent<UITextRenderer>(btnEntity);
		btnText.text = "PLAY";
		btnText.material = 2;
		auto& txtBtn = reg.addComponent<TextButton>(btnEntity);
		txtBtn.clickPadding = 6.0f;
		txtBtn.hoverMaterial = 5;

		const Entity tglEntity = reg.createEntity();
		reg.addComponent<Transform>(tglEntity).position = vec3(500.0f, 300.0f, 0.0f);
		auto& tglText = reg.addComponent<UITextRenderer>(tglEntity);
		tglText.text = "MUTE";
		tglText.material = 3;
		auto& txtTgl = reg.addComponent<TextToggle>(tglEntity);
		txtTgl.active = false;
		txtTgl.activeMaterial = 7;
		txtTgl.inactiveMaterial = 3;
		txtTgl.hoverMaterial = 8;

		// Input::GetMouseY() = Display::height - m_mouseY, so screen UI Y=310 maps to SDL Y=490 at 800px tall
		Input::SetMousePosition(140.0f, 490.0f);
		ButtonSystem::updateTextButtons(reg, uiCtx);
		ButtonSystem::updateTextToggles(reg, uiCtx);
		require(txtBtn.hovered && btnText.material == 5 && !txtTgl.hovered && tglText.material == 3,
				"Hovering TextButton must swap to hoverMaterial while leaving unhovered TextToggle inactiveMaterial.");

		Input::SetMousePosition(540.0f, 490.0f);
		ButtonSystem::updateTextButtons(reg, uiCtx);
		ButtonSystem::updateTextToggles(reg, uiCtx);
		require(!txtBtn.hovered && btnText.material == 2 && txtTgl.hovered && tglText.material == 8,
				"Leaving TextButton must restore baseMaterial and hovering TextToggle must apply hoverMaterial.");

		// Verify TextButton requireClickStart (default true vs false) with Registry updateTextButtons
		txtBtn.pressedMaterial = 9;
		Input::clearUIClick();
		Input::SetMousePosition(140.0f, 490.0f); // over PLAY TextButton
		ButtonSystem::updateTextButtons(reg, uiCtx, true, false);
		require(txtBtn.hovered && txtBtn.state == ButtonState::Off && btnText.material == 5,
				"TextButton with default requireClickStart must not click when hovered while mouse is already held.");

		txtBtn.requireClickStart = false;
		Input::clearUIClick();
		ButtonSystem::updateTextButtons(reg, uiCtx, true, false);
		require(txtBtn.hovered && txtBtn.state == ButtonState::Down && btnText.material == 9,
				"TextButton with requireClickStart=false must trigger Down when hovered while mouse is held.");
		Input::clearUIClick();
		ButtonSystem::updateTextButtons(reg, uiCtx, true, false);
		require(txtBtn.state == ButtonState::Hold && btnText.material == 9,
				"TextButton with requireClickStart=false must advance to Hold while hovered and held.");
		Input::SetMousePosition(0.0f, 0.0f); // leave PLAY TextButton while still holding mouse
		Input::clearUIClick();
		ButtonSystem::updateTextButtons(reg, uiCtx, true, false);
		require(!txtBtn.hovered && txtBtn.state == ButtonState::Up && btnText.material == 2,
				"TextButton with requireClickStart=false must stop being clicked and restore baseMaterial on leave.");
		Input::clearUIClick();
		ButtonSystem::updateTextButtons(reg, uiCtx, true, false);
		require(txtBtn.state == ButtonState::Off,
				"TextButton with requireClickStart=false must settle Up -> Off after leaving.");

		// Verify ShapeButton requireClickStart (default true vs false) with Registry updateButtons
		Scene::registerBuiltinSDFs();
		auto builtinSpan = Scene::getBuiltinSDFs();
		std::vector<std::shared_ptr<IMathExpression>> builtins(builtinSpan.begin(), builtinSpan.end());
		Registry shapeReg;
		shapeReg.registerComponent<UIShape>();
		shapeReg.registerComponent<ShapeButton>();

		const Entity shapeBtnEntity = shapeReg.createEntity();
		auto& uiShape = shapeReg.addComponent<UIShape>(shapeBtnEntity);
		uiShape.distanceFieldId = DefaultShapes::Box;
		uiShape.parameters[DefaultShapes::Box::PosX] = 200.0f;
		uiShape.parameters[DefaultShapes::Box::PosY] = 200.0f;
		uiShape.parameters[DefaultShapes::Box::SizeX] = 40.0f;
		uiShape.parameters[DefaultShapes::Box::SizeY] = 20.0f;
		auto& shapeBtn = shapeReg.addComponent<ShapeButton>(shapeBtnEntity);
		shapeBtn.clickPadding = 0.0f;
		shapeBtn.modifierAmount = 5.0f;
		shapeBtn.parameterModifierMask.set(DefaultShapes::Box::SizeX);

		// Screen UI Y=200 maps to SDL Y=600 at 800px height
		Input::SetMousePosition(200.0f, 600.0f);
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, false);
		require(shapeBtn.hovered && shapeBtn.state == ButtonState::Off &&
					std::abs(uiShape.parameters[DefaultShapes::Box::SizeX] - 40.0f) < 0.01f,
				"ShapeButton with default requireClickStart must ignore hovering while mouse is already held.");

		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, true);
		require(shapeBtn.state == ButtonState::Down &&
					std::abs(uiShape.parameters[DefaultShapes::Box::SizeX] - 45.0f) < 0.01f,
				"ShapeButton with default requireClickStart must trigger when click starts on hover.");
		Input::SetMousePosition(0.0f, 0.0f);
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, false);
		require(!shapeBtn.hovered && shapeBtn.state == ButtonState::Hold,
				"ShapeButton with default requireClickStart must stay Hold while mouse remains pressed.");
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, false, false);
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, false, false);
		require(shapeBtn.state == ButtonState::Off &&
					std::abs(uiShape.parameters[DefaultShapes::Box::SizeX] - 40.0f) < 0.01f,
				"ShapeButton must settle to Off and revert parameter modifier after mouse release.");

		shapeBtn.requireClickStart = false;
		Input::SetMousePosition(200.0f, 600.0f);
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, false);
		require(shapeBtn.hovered && shapeBtn.state == ButtonState::Down &&
					std::abs(uiShape.parameters[DefaultShapes::Box::SizeX] - 45.0f) < 0.01f,
				"ShapeButton with requireClickStart=false must trigger when hovered while mouse is held.");
		Input::SetMousePosition(0.0f, 0.0f);
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, false);
		require(!shapeBtn.hovered && shapeBtn.state == ButtonState::Up,
				"ShapeButton with requireClickStart=false must stop being clicked as soon as mouse leaves shape.");
		ButtonSystem::updateButtons(shapeReg, builtins, 0.0f, true, false);
		require(shapeBtn.state == ButtonState::Off &&
					std::abs(uiShape.parameters[DefaultShapes::Box::SizeX] - 40.0f) < 0.01f,
				"ShapeButton with requireClickStart=false must settle Off and revert modifier after leaving.");

		Input::clearUIClick();
		Input::SetMousePosition(0.0f, 0.0f);
	}
} // namespace

int main(int argc, char** argv)
{
	return WeirdTests::runTests(argc, argv, "WeirdUITests",
								{
									{"TextButtonAndToggleComponents", testTextButtonAndToggleComponents},
								});
}
