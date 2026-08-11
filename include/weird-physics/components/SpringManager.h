#pragma once

#include "weird-engine/ecs/ComponentManager.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	class SpringManager : public ComponentManager<Spring>
	{
	private:
		Simulation2D* m_simulation;
		Registry* m_registry;

	public:
		SpringManager(Simulation2D& simulation, Registry& registry)
			: m_simulation(&simulation)
			, m_registry(&registry)
		{
		}

		void handleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<Spring>>(m_componentArray);
			Spring& removedSpring = componentArray->getDataFromEntity(entity);

			if (m_registry->hasComponent<RigidBody2D>(removedSpring.entityA) &&
				m_registry->hasComponent<RigidBody2D>(removedSpring.entityB))
			{
				auto simIdA = m_registry->getComponent<RigidBody2D>(removedSpring.entityA).simulationId;
				auto simIdB = m_registry->getComponent<RigidBody2D>(removedSpring.entityB).simulationId;
				// DistanceConstraints and Springs are treated identically for removal in Simulation2D
				m_simulation->removeDistanceConstraint(simIdA, simIdB);
			}
		}
	};
} // namespace WeirdEngine
