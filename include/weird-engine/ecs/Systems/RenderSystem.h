#pragma once
#include "../ECS.h"
#include "../../ResourceManager.h"

namespace WeirdEngine
{
	using namespace ECS;

	class RenderSystem : public System {
	private:
		std::shared_ptr<ComponentManager<MeshRenderer>> m_meshRendererManager;

	public:

		RenderSystem(ECSManager& ecs) {
			m_meshRendererManager = ecs.getComponentManager<MeshRenderer>();
		}

		void render(ECSManager& ecs, ResourceManager& resourceManager, WeirdRenderer::Shader& shader, WeirdRenderer::Camera& camera, const std::vector<WeirdRenderer::Light>& lights) {

			shader.use();

			auto componentArray = m_meshRendererManager->getComponentArray();

			for (size_t i = 0; i < componentArray->getSize(); i++)
			{
				MeshRenderer& mr = componentArray->getDataAtIdx(i);
				auto& t = ecs.getComponent<Transform>(mr.Owner);

				resourceManager.getMesh(mr.mesh).draw(shader, camera, t.position, t.rotation, t.scale);
			}
		}
	};
}