#pragma once
#pragma once
#include "../Component.h"


namespace ECS
{
	struct FlyMovement2D : public Component 
	{

		float speed;
		float scrollSpeed = 10.1f;

		FlyMovement2D() : speed(10.0f) 
		{

		}



	};
}
