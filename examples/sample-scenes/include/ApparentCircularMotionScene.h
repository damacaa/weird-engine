#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class ApparentCircularMotionScene : public Scene
{
private:
	// Inherited via Scene
	uint16_t m_count = 8;
	std::vector<Entity> m_circles;

	void onStart() override
	{
		m_simulation2D.setDamping(0.0f);
		m_simulation2D.setGravity(0.0f);

		for (size_t i = 0; i < m_count; i++)
		{
			Entity entity = m_ecs.createEntity();

			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(0, 0, 0);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = i % 16;
			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);

			m_circles.push_back(entity);
		}
	}

	void onUpdate(float delta) override
	{
		auto time = getTime();

		if (time > 1)
		{
			// return;
		}

		for (size_t i = 0; i < m_count; i++)
		{
			float angle = 1 * 3.1416 * i / m_count;
			auto& t = m_ecs.getComponent<Transform>(m_circles[i]);
			t.position = (10.0f * ((float)sin(time + angle)) * vec3(cos(angle), sin(angle), 0)) + vec3(15, 15, 0);
			t.isDirty = true;
		}
	}
};
