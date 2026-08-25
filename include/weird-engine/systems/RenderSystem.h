#pragma once
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/ResourceManager.h"
#include "weird-renderer/components/Light2DComponent.h"
#include "weird-renderer/components/Light3DComponent.h"
#include "weird-renderer/resources/DrawCommand.h"
#include "weird-renderer/scene/Light.h"
#include <glm/gtc/constants.hpp>
#include <vector>

namespace WeirdEngine
{
	using namespace ECS;

	namespace RenderSystem
	{
		inline void update(Registry& registry, ResourceManager& resourceManager,
						   std::vector<WeirdRenderer::DrawCommand>& drawQueue,
						   std::vector<WeirdRenderer::Light2D>& lights2D, std::vector<WeirdRenderer::Light3D>& lights3D)
		{
			drawQueue.clear();
			lights2D.clear();
			lights3D.clear();

			registry.forEach<MeshRenderer, Transform>(
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

			registry.forEach<Light2DComponent, Transform>(
				[&](Entity mOwner, Light2DComponent& lc, Transform& t)
				{
					WeirdRenderer::Light2D light;
					light.type = static_cast<uint32_t>(lc.type);
					light.position = glm::vec2(t.position.x, t.position.y);
					light.direction = lc.direction;
					light.color = lc.color;
					light.radius = lc.radius;
					light.coneAngle = glm::radians(lc.coneAngle);
					light.conePenumbra = glm::radians(lc.conePenumbra);
					light.castShadows = lc.castShadows ? 1 : 0;

					lights2D.push_back(light);
				});

			registry.forEach<Light3DComponent, Transform>(
				[&](Entity mOwner, Light3DComponent& lc, Transform& t)
				{
					WeirdRenderer::Light3D light;
					light.type = static_cast<uint32_t>(lc.type);
					light.color = lc.color;
					light.position = t.position;
					light.rotation = t.rotation;

					lights3D.push_back(light);
				});
		}
	} // namespace RenderSystem
} // namespace WeirdEngine
