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
		SDFRenderSystemContext* m_renderContext;

	public:
		CustomShapeManager(Simulation2D& simulation, SDFRenderSystemContext& renderContext)
			: m_simulation(&simulation), m_renderContext(&renderContext)
		{
		}

		// Can't add the shape to the simulation here because the component data is not initialized yet, so we will add it in the next update of the PhysicsSystem2D
		void handleNewComponent(Entity entity, CustomShape& component) override
		{
			m_renderContext->shapesNeedUpdate = true;
		}

		void handleDestroyedComponent(Entity entity) override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<CustomShape>>(m_componentArray);
			CustomShape& removedShape = componentArray->getDataFromEntity(entity);
			m_simulation->removeShape(removedShape);

			m_renderContext->shapesNeedUpdate = true;
		}
	};

	class CustomUIShapeManager : public ComponentManager<UIShape>
	{
	private:
	SDFRenderSystemContext* m_renderContext;


	public:
		CustomUIShapeManager(SDFRenderSystemContext& context)
			: m_renderContext(&context)
		{
		}

		void handleNewComponent(Entity entity, UIShape& component) override
		{
			m_renderContext->shapesNeedUpdate = true;
		}

		void handleDestroyedComponent(Entity entity) override
		{
			m_renderContext->shapesNeedUpdate = true;
		}
	};
} // namespace WeirdEngine