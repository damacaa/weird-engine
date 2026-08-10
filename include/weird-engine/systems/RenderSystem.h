#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ResourceManager.h"

#include "weird-renderer/resources/DrawCommand.h"
#include "weird-renderer/scene/Light.h"
#include <vector>

namespace WeirdEngine
{
	using namespace ECS;

	namespace RenderSystem
	{
		inline void update(ECSManager& ecs, ResourceManager& resourceManager,
						   std::vector<WeirdRenderer::DrawCommand>& drawQueue,
						   std::vector<WeirdRenderer::Light>& lights)
		{
			drawQueue.clear();
			lights.clear();

			ecs.forEach<MeshRenderer, Transform>(
				[&](Entity mOwner, MeshRenderer& mr, Transform& t)
				{
					WeirdRenderer::DrawCommand cmd;
					cmd.mesh = &resourceManager.getMesh(mr.mesh);
					cmd.materialIndex = mr.materialIndex;
					cmd.translation = t.position;
					cmd.rotation = t.rotation;
					cmd.scale = t.scale;

					drawQueue.push_back(cmd);
				});

			ecs.forEach<LightComponent, Transform>(
				[&](Entity mOwner, LightComponent& lc, Transform& t)
				{
					WeirdRenderer::Light light;
					light.type = static_cast<uint32_t>(lc.type);
					light.color = lc.color;
					light.position = t.position;
					light.rotation = t.rotation;

					lights.push_back(light);
				});
		}
	} // namespace RenderSystem
} // namespace WeirdEngine
