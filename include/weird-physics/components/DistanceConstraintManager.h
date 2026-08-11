#pragma once

#include "weird-engine/ecs/ComponentManager.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	class DistanceConstraintManager : public ComponentManager<DistanceConstraint>
	{
	private:
		Simulation2D* m_simulation;
		Registry* m_registry;

	public:
		DistanceConstraintManager(Simulation2D& simulation, Registry& registry)
			: m_simulation(&simulation)
			, m_registry(&registry)
		{
		}

		void handleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<DistanceConstraint>>(m_componentArray);
			DistanceConstraint& removedConstraint = componentArray->getDataFromEntity(entity);

			if (m_registry->hasComponent<RigidBody2D>(removedConstraint.entityA) &&
				m_registry->hasComponent<RigidBody2D>(removedConstraint.entityB))
			{
				auto simIdA = m_registry->getComponent<RigidBody2D>(removedConstraint.entityA).simulationId;
				auto simIdB = m_registry->getComponent<RigidBody2D>(removedConstraint.entityB).simulationId;
				m_simulation->removeDistanceConstraint(simIdA, simIdB);
			}
		}
	};
} // namespace WeirdEngine
