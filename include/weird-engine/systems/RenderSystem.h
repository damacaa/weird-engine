#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ResourceManager.h"

#include "weird-renderer/resources/DrawCommand.h"
#include <vector>

namespace WeirdEngine
{
	using namespace ECS;

	namespace RenderSystem
	{
		inline void update(ECSManager& ecs, ResourceManager& resourceManager, std::vector<WeirdRenderer::DrawCommand>& drawQueue)
		{
			drawQueue.clear();

			auto componentArray = ecs.getComponentManager<MeshRenderer>()->getComponentArray();

			for (size_t i = 0; i < componentArray->getSize(); i++)
			{
				MeshRenderer& mr = componentArray->getDataAtIdx(i);
				auto& t = ecs.getComponent<Transform>(mr.Owner);

				WeirdRenderer::DrawCommand cmd;
				cmd.mesh = &resourceManager.getMesh(mr.mesh);
				cmd.materialIndex = mr.materialIndex;
				cmd.translation = t.position;
				cmd.rotation = t.rotation;
				cmd.scale = t.scale;

				drawQueue.push_back(cmd);
			}
		}
	} // namespace RenderSystem
} // namespace WeirdEngine