#pragma once
#pragma once
#include "../Component.h"

namespace WeirdEngine
{
	namespace ECS
	{
		struct FlyMovement2D : public Component
		{

			float speed;
			float scrollSpeed = 5.0f;

			FlyMovement2D() : speed(1.0f)
			{

			}



		};
	}
}