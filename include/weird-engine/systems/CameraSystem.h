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
					Camera& c = componentArray[i];
					Transform& t = ecs.getComponent<Transform>(c.Owner);

					c.camera.position = t.position;
					c.camera.orientation = t.rotation;
				}
			}
		}
	}
}