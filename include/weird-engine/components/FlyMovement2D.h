#pragma once
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace ECS
	{
		struct FlyMovement2D
		{
			vec3 targetPosition;
			float scrollSpeed = 5.0f;
			vec3 v;
			float speed = 1.0f;
			bool isSmooth = true;
		};
	} // namespace ECS
} // namespace WeirdEngine