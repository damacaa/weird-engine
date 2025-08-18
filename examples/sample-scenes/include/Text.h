#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class TextScene : public WeirdEngine:: Scene
{

private:



	// Inherited via Scene
	void onStart() override
	{
		std::string example("Hello World!");

		print(example);
	}

	void onUpdate(float delta) override
	{
	}
};