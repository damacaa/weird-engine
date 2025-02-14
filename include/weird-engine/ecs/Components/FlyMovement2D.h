#pragma once
#pragma once
#include "../Component.h"

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

			FlyMovement2D() : speed(1.0f)
			{

			}



		};
	}
}