#pragma once
#include "../ECS.h"

class RenderSystem : public System {

public:
	RenderSystem() {

		m_entities = std::vector<Entity>();

	}


	void render(ECS& ecs, Shader& shader, Camera& camera, const std::vector<Light>& lights) {

		auto& componentArray = GetManagerArray<MeshRenderer>();

		for (size_t i = 0; i < componentArray.getSize(); i++)
		{
			MeshRenderer& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			mr.mesh.Draw(shader, camera, t.position, t.rotation, t.scale, lights);
		}
	}
};