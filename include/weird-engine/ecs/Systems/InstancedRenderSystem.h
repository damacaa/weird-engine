#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"


namespace WeirdEngine
{
using namespace ECS;
	class InstancedRenderSystem : public System {
	private:
		const size_t MAX_INSTANCES = 255;
		std::shared_ptr<ComponentManager<InstancedMeshRenderer>> m_iRendererManager;

	public:

		InstancedRenderSystem(ECSManager& ecs)
		{
			m_iRendererManager = ecs.getComponentManager<InstancedMeshRenderer>();
		}

		void render(ECSManager& ecs, ResourceManager& resourceManager, WeirdRenderer::Shader& shader, WeirdRenderer::Camera& camera, const std::vector<WeirdRenderer::Light>& lights) 
		{

		}
	};
}