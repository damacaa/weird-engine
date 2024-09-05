#pragma once
#pragma once
#include "../ECS.h"
#include "../../Input.h"


namespace ECS
{
	class CameraSystem : public System
	{
	private:

	public:

		CameraSystem(ECSManager& ecs)
		{

		}

		void update(ECSManager& ecs, Entity mainCamera)
		{
			Transform& t = ecs.getComponent<Transform>(mainCamera);
			Camera& c = ecs.getComponent<Camera>(mainCamera);

			c.camera.Position = t.position;
			c.camera.Orientation = t.rotation;
		}

	private:


	};
}