#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderTarget.h"

namespace WeirdEngine
{
	using namespace ECS;

	class SDFRenderSystem : public System {
	private:
		std::shared_ptr<ComponentManager<SDFRenderer>> m_sdfRendererManager;
		std::shared_ptr<ComponentManager<Transform>> m_transformManager;

	public:

		SDFRenderSystem(ECSManager& ecs) {
			m_sdfRendererManager = ecs.getComponentManager<SDFRenderer>();
			m_transformManager = ecs.getComponentManager<Transform>();
		}

		void render(ECSManager& ecs, WeirdRenderer::Shader& shader, WeirdRenderer::RenderTarget& rp, const std::vector< WeirdRenderer::Light>& lights) {

			auto componentArray = m_sdfRendererManager->getComponentArray();
			auto transformArray = m_transformManager->getComponentArray();

			unsigned int size = componentArray->getSize();

			WeirdRenderer::Shape* data = new WeirdRenderer::Shape[size];

			for (size_t i = 0; i < size; i++)
			{
				auto& mr = componentArray->getDataAtIdx(i);
				//auto& t = ecs.getComponent<Transform>(mr.Owner);
				auto& t = transformArray->getDataFromEntity(mr.Owner);

				data[i].position = t.position;// +glm::vec3(0, mr.Owner, 0);
				//data[i].size = t.scale.x;
			}

			shader.setUniform("directionalLightDirection", lights[0].rotation);
			//rp.draw(shader, data, size);

			delete[] data;

			//rp->draw(shader, m_data, m_shapes);
		}
	};
}