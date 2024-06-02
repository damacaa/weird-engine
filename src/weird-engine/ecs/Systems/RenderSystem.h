#pragma once
#include "../ECS.h"

class RenderSystem : public System {
public:
	void render(ECS& ecs, Shader& shader, Camera& camera, const std::vector<Light>& lights) {

		shader.activate();
		shader.setUniform("lightColor", lights[0].color);
		shader.setUniform("lightPos", lights[0].position);

		auto& componentArray = GetManagerArray<MeshRenderer>();

		for (size_t i = 0; i < componentArray.getSize(); i++)
		{
			MeshRenderer& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			mr.mesh.Draw(shader, camera, t.position, t.rotation, t.scale, lights);
		}
	}
};