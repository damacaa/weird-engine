#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/vec.h"
#include "weird-renderer/core/RenderTarget.h"

namespace WeirdEngine
{
	using namespace ECS;

	namespace SDFRenderSystem
	{

		inline void update(ECSManager& ecs, WeirdRenderer::Shader& shader, WeirdRenderer::RenderTarget& rp,
						   const std::vector<WeirdRenderer::Light>& lights)
		{

			auto componentArray = ecs.getComponentManager<Dot>()->getComponentArray();
			auto transformArray = ecs.getComponentManager<Transform>()->getComponentArray();

			unsigned int size = componentArray->getSize();

			vec4* data = new vec4[size];

			for (size_t i = 0; i < size; i++)
			{
				auto& mr = componentArray->getDataAtIdx(i);
				// auto& t = ecs.getComponent<Transform>(mr.Owner);
				auto& t = transformArray->getDataFromEntity(mr.Owner);

				data[i].x = t.position.x;
				data[i].y = t.position.y;
				data[i].z = t.position.z;
				data[i].w = mr.materialId;
			}

			shader.setUniform("directionalLightDirection", lights[0].rotation);
			// rp.draw(shader, data, size);

			delete[] data;

			// rp->draw(shader, m_data, m_shapes);
		}
	} // namespace SDFRenderSystem
} // namespace WeirdEngine