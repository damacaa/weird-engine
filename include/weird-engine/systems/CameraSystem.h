#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"

namespace WeirdEngine
{
	namespace ECS
	{
		namespace CameraSystem
		{
			inline void update(ECSManager& ecs)
			{
				ecs.forEach<Camera, Transform>(
					[](Entity camOwner, Camera& c, Transform& t)
					{
						c.camera.position = t.position;
						c.camera.orientation = t.rotation;
						c.camera.nearPlane = c.nearPlane;
						c.camera.farPlane = c.farPlane;
					});
			}
		} // namespace CameraSystem
	} // namespace ECS
} // namespace WeirdEngine