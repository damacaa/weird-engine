#pragma once
#pragma once
#include "../Component.h"

namespace WeirdEngine
{
	namespace ECS
	{
		struct FlyMovement : public Component
		{

			float speed;
			float sensitivity = 50000.0f;

			FlyMovement() : speed(10.0f)
			{

			}



		};
	}
}