#pragma once
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/Input.h"

namespace WeirdEngine
{
	namespace ECS
	{
		namespace CameraSystem
		{
			inline void update(Registry& registry)
			{
				registry.forEach<Camera, Transform>(
					[](Entity camOwner, Camera& c, Transform& t)
					{
						c.camera.position = t.position;
						if (glm::dot(t.rotation, t.rotation) > 0.0001f)
						{
							c.camera.orientation = glm::normalize(t.rotation);
						}
						else
						{
							c.camera.orientation = glm::vec3(0.0f, 0.0f, -1.0f);
							t.rotation = glm::vec3(0.0f, 0.0f, -1.0f);
						}
						c.camera.nearPlane = c.nearPlane;
						c.camera.farPlane = c.farPlane;
					});
			}
		} // namespace CameraSystem
	} // namespace ECS
} // namespace WeirdEngine
