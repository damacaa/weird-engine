#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"

namespace WeirdEngine
{
	namespace ECS
	{
		class CameraSystem : public System
		{
		private:

		public:

			CameraSystem(ECSManager& ecs)
			{

			}

			void update(ECSManager& ecs)
			{
				auto& componentArray = *ecs.getComponentManager<Camera>()->getComponentArray();
				unsigned int size = componentArray.getSize();
				for (size_t i = 0; i < size; i++)
				{
					Camera& c = componentArray[i];
					Transform& t = ecs.getComponent<Transform>(c.Owner);

					c.camera.Position = t.position;
					c.camera.Orientation = t.rotation;
				}
			}
		};
	}
}