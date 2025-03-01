#pragma once

#include "RigidBody.h"
#include "weird-engine/ecs/ComponentManager.h"

namespace WeirdEngine
{
	class RigidBodyManager : public ComponentManager<RigidBody2D>
	{
	private:
		Simulation2D* m_simulation;

	public:

		RigidBodyManager(Simulation2D& simulation)
			: m_simulation(&simulation) {}

		void HandleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<RigidBody2D>>(m_componentArray);
			RigidBody2D rb = componentArray->getDataFromEntity(entity);
			m_simulation->removeObject(rb.simulationId);
		}
	};
}