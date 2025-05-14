#pragma once
#include "weird-engine/ecs/Components/RigidBody.h"
#include "weird-engine/ecs/Components/Transform.h"
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/ecs/System.h"
#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	class PhysicsSystem2D : public System
	{
	private:
		std::shared_ptr<ComponentManager<RigidBody2D>> m_rbManager;
		std::shared_ptr<ComponentManager<Transform>> m_transformManager;
		Simulation2D* m_simulation = nullptr;

	public:
		PhysicsSystem2D(Simulation2D& sim)
			: m_simulation(&sim)
		{
		}

		void cacheComponents(ECSManager& ecs) override
		{
			m_rbManager = ecs.getComponentManager<RigidBody2D>();
			m_transformManager = ecs.getComponentManager<Transform>();
		}

		void update(ECSManager& ecs, float dt) override
		{
			auto rbArray = m_rbManager->getComponentArray();
			auto transformArray = m_transformManager->getComponentArray();

			for (size_t i = 0; i < rbArray->getSize(); i++)
			{
				RigidBody2D& rb = rbArray->getDataAtIdx(i);
				Transform& transform = transformArray->getDataFromEntity(rb.Owner);

				// Sync position
				transform.position = rb.position;
				transform.isDirty = false;

				// Update simulation
				m_simulation->updateTransform(rb.simulationId, transform.position);
			}
		}
	};
}
