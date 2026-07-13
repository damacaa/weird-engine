#pragma once
#include <weird-engine.h>

class TextScene : public WeirdEngine::Scene2D
{
public:
	void onStart(WeirdEngine::ECSManager& ecs) override
	{
		auto e1 = ecs.createEntity();
		auto& text = ecs.addComponent<WeirdEngine::TraditionalTextComponent>(e1);
		text.text = "Hello World SDF!";
		text.fontSize = 64.0f;
		text.fontPath = "/usr/share/fonts/google-noto/NotoSerif-SemiCondensedLightItalic.ttf";
		text.materialId = 0;
		text.dirty = true;
		
		auto& transform = ecs.addComponent<WeirdEngine::Transform>(e1);
		transform.position = {10.0f, 10.0f, 0.0f};

        auto e2 = ecs.createEntity();
		auto& text2 = ecs.addComponent<WeirdEngine::TraditionalTextComponent>(e2);
		text2.text = "Second Line Text!";
		text2.fontSize = 32.0f;
		text2.fontPath = "/usr/share/fonts/google-noto/NotoSerif-SemiCondensedLightItalic.ttf";
		text2.materialId = 1;
		text2.dirty = true;
		
		auto& transform2 = ecs.addComponent<WeirdEngine::Transform>(e2);
		transform2.position = {5.0f, 20.0f, 0.0f};


	}

	void onUpdate(float delta, WeirdEngine::ECSManager& ecs) override
	{
	}
};
