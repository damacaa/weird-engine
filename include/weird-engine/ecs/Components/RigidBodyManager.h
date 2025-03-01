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
			auto componentArray = std::static_pointer_cast<ComponentArray<RigidBody2D>>(m_componentArray);
		}

		void HandleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<RigidBody2D>>(m_componentArray);

			// Get removed component
			RigidBody2D& removedRb = componentArray->getDataFromEntity(entity);
			// Get its simulationId
			auto removedId = removedRb.simulationId;

			// Get last component in array, which will be moved to removedRb position
			RigidBody2D& lastRb = componentArray->getLastData();

			m_simulation->removeObject(removedId);

			lastRb.simulationId = removedId;

			// TODO: lastRb.simulationId might not match the lastId used in m_simulation->removeObject !!!!
		}

		void HandlePostDestroyedComponent(Entity entity) override
		{
		
			/*std::cout << "After destroy: " << std::endl;

			auto componentArray = std::static_pointer_cast<ComponentArray<RigidBody2D>>(m_componentArray);

		
			for (size_t i = 0; i < componentArray->getSize(); i++)
			{
				auto& c = componentArray->getDataAtIdx(i);
				std::cout << "  -" << i << " Entity: " << c.Owner << " Id:" << c.simulationId << std::endl;
			}

			std::cout << "------------------" << std::endl;*/
		}
	};
}