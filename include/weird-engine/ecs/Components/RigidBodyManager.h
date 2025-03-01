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

		void HandleNewComponent(Entity entity, RigidBody2D& component) override
		{
			component.simulationId = m_simulation->generateSimulationID();
		}

		void HandleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<RigidBody2D>>(m_componentArray);

			// Get removed component
			RigidBody2D& removedRb = componentArray->getDataFromEntity(entity);
			// Get its simulationId
			auto removedId = removedRb.simulationId;

			// Get last component in array, which will be moved to removedRb position
			RigidBody2D& lastRb = componentArray->getDataAtIdx(componentArray->getSize() - 1);

			componentArray->replaceData(lastRb.Owner, removedRb.Owner);

			m_simulation->removeObject(removedId);
		}
	};
}