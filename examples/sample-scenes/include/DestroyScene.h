#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class DestroyScene : public Scene
{
public:
	DestroyScene()
		: Scene() {
	};

private:
	std::array<Entity, 10> m_testEntity;
	bool m_testEntityCreated = false;

	float m_lastSpawnTime = 0.0f;

	void onUpdate(float delta) override
	{
		if (m_testEntityCreated && (getTime() - m_lastSpawnTime) > 0.5f && Input::GetKeyDown(Input::U))
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				m_ecs.destroyEntity(m_testEntity[i]);
			}

			m_testEntityCreated = false;
		}
		else if (!m_testEntityCreated)
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				Entity entity = m_ecs.createEntity();
				Transform& t = m_ecs.addComponent<Transform>(entity);
				t.position = vec3(15.0f + (i % 10), 30.0f + (i / 10), 0.0f);
				t.isDirty = true;

				SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
				sdfRenderer.materialId = 4 + m_ecs.getComponentArray<SDFRenderer>()->getSize() % 12;

				RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
				m_simulation2D.addForce(rb.simulationId, vec2(0, -50));

				m_testEntity[i] = entity;
			}

			m_testEntityCreated = true;
			m_lastSpawnTime = getTime();
		}

		if (m_testEntityCreated && Input::GetKeyDown(Input::I))
		{
			for (size_t i = 0; i < m_testEntity.size(); i++)
			{
				RigidBody2D& rb = m_ecs.getComponent<RigidBody2D>(m_testEntity[i]);

				if (i > 0)
				{
					m_simulation2D.addSpring(rb.simulationId, rb.simulationId - 1, 1000000.0f);
				}
				else
				{
					m_simulation2D.setPosition(rb.simulationId, vec2(0, 15));
					m_simulation2D.fix(rb.simulationId);
				}
			}
		}
	}

	// Inherited via Scene
	void onStart() override
	{
		{
			float variables[8]{ 0.0f, 0.0f };
			addShape(0, variables, 3);
		}
	}
};
