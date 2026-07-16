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
				auto& componentArray = *ecs.getComponentManager<Camera>()->getComponentArray();
				unsigned int size = componentArray.getSize();
				for (size_t i = 0; i < size; i++)
				{
					auto& c = componentArray.getDataAtIdx(i);
					Entity camOwner = componentArray.getEntityAtIdx(i);
					Transform& t = ecs.getComponent<Transform>(camOwner);

					c.camera.position = t.position;
					c.camera.orientation = t.rotation;
					c.camera.nearPlane = c.nearPlane;
					c.camera.farPlane = c.farPlane;
				}
			}
		} // namespace CameraSystem
	} // namespace ECS
} // namespace WeirdEngine