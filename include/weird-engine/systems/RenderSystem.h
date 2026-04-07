#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ResourceManager.h"

namespace WeirdEngine
{
	using namespace ECS;

	namespace RenderSystem
	{

		inline void update(ECSManager& ecs, ResourceManager& resourceManager, WeirdRenderer::Shader& shader,
						   WeirdRenderer::Camera& camera, const std::vector<WeirdRenderer::Light>& lights)
		{

			shader.use();

			auto componentArray = ecs.getComponentManager<MeshRenderer>()->getComponentArray();

			for (size_t i = 0; i < componentArray->getSize(); i++)
			{
				MeshRenderer& mr = componentArray->getDataAtIdx(i);
				auto& t = ecs.getComponent<Transform>(mr.Owner);

				resourceManager.getMesh(mr.mesh).draw(shader, camera, t.position, t.rotation, t.scale);
			}
		}
	} // namespace RenderSystem
} // namespace WeirdEngine