#pragma once
#include "weird-engine/ecs/Component.h"
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	namespace ECS
	{
		struct FlyMovement2D : public Component
		{

			vec3 targetPosition;
			float scrollSpeed = 5.0f;
			vec3 v;
			float speed;
			bool isSmooth = true;

			FlyMovement2D()
				: speed(1.0f)
			{
			}
		};
	} // namespace ECS
} // namespace WeirdEngine