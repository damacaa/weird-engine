#pragma once

#include "weird-engine/ecs/ComponentManager.h"
#include "weird-physics/Simulation2D.h"
#include "weird-renderer/components/CustomShape.h"

namespace WeirdEngine
{
	class CustomShapeManager : public ComponentManager<CustomShape>
	{
	private:
		Simulation2D* m_simulation;

	public:
		CustomShapeManager(Simulation2D& simulation)
			: m_simulation(&simulation) {}

		void handleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<CustomShape>>(m_componentArray);
			CustomShape& removedShape = componentArray->getDataFromEntity(entity);
			m_simulation->removeShape(removedShape);
		}
	};
}