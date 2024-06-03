#pragma once
#include "../ECS.h"
#include "../../../weird-renderer/RenderPlane.h"

class SDFRenderSystem : public System {

public:
	void render(ECS& ecs, Shader& shader, RenderPlane& rp, const std::vector<Light>& lights) {

		auto& componentArray = GetManagerArray<SDFRenderer>();

		unsigned int size = componentArray.getSize();

		Shape* data = new Shape[size];

		for (size_t i = 0; i < size; i++)
		{
			auto& mr = componentArray[i];
			auto& t = ecs.getComponent<Transform>(mr.Owner);

			data[i].position = t.position;// +glm::vec3(0, mr.Owner, 0);
			//data[i].size = t.scale.x;
		}

		shader.setUniform("directionalLightDirection", lights[0].rotation);
		rp.Draw(shader, data, size);

		delete[] data;

		//rp->Draw(shader, m_data, m_shapes);
	}
};