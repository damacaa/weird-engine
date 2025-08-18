#pragma once

#include <weird-engine.h>
#include <random>

using namespace WeirdEngine;
class SpaceScene : public Scene
{
private:
	std::vector<Entity> m_celestialBodies;
	uint16_t m_current = 0;
	bool m_lookAtBody = true;

	// Inherited via Scene
	void onStart() override
	{
		// m_debugFly = false;

		m_simulation2D.setGravity(0);
		m_simulation2D.setDamping(0);

		loadRandomSystem();
	}

	void loadRandomSystem()
	{

		std::random_device rd;
		std::mt19937 gen(rd());

		std::uniform_real_distribution<float> floatDistrib(-1, 1);
		std::uniform_real_distribution<float> massDistrib(0.1f, 100.0f);
		std::uniform_int_distribution<int> colorDistrib(2, 15);

		size_t bodyCount = 1000;
		float r = 0;
		for (size_t i = 0; i < bodyCount; i++)
		{

			Entity body = m_ecs.createEntity();

			vec2 pos(floatDistrib(gen), floatDistrib(gen));
			Transform& t = m_ecs.addComponent<Transform>(body);
			t.position = 500.0f * vec3(pos.x, pos.y, 0);

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(body);
			sdfRenderer.materialId = 4 + (i % 3);

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(body);
			m_simulation2D.setMass(rb.simulationId, 1.0f);

			for (auto b : m_celestialBodies)
			{
				m_simulation2D.addGravitationalConstraint(
					m_ecs.getComponent<RigidBody2D>(body).simulationId,
					m_ecs.getComponent<RigidBody2D>(b).simulationId,
					100.0f);
			}

			m_simulation2D.addForce(m_ecs.getComponent<RigidBody2D>(body).simulationId, (10.f * vec2(-pos.y, pos.x)) - (10.0f * vec2(pos.x, pos.y)));

			m_celestialBodies.push_back(body);
		}

		lookAt(m_celestialBodies[0]);
	}

	void onUpdate(float delta) override
	{
		if (Input::GetKeyDown(Input::E))
		{
			m_current = (m_current + 1) % m_celestialBodies.size();
		}

		if (Input::GetKeyDown(Input::F))
		{
			m_lookAtBody = !m_lookAtBody;
		}

		if (m_lookAtBody && m_celestialBodies.size() > 0)
		{
			lookAt(m_celestialBodies[m_current]);
		}
	}
};
