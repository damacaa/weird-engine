#pragma once
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace ECS
	{
		struct FlyMovement2D
		{
			vec3 targetPosition = vec3(0.0f);
			float scrollSpeed = 5.0f;
			vec3 v = vec3(0.0f);
			float speed = 1.0f;
			bool isSmooth = true;
		};
	} // namespace ECS
} // namespace WeirdEngine