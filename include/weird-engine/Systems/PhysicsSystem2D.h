#pragma once
#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	namespace ECS
	{



		// Example Systems
		class PhysicsSystem2D : public System
		{
		private:
			std::shared_ptr<ComponentManager<RigidBody2D>> m_rbManager;

		public:

			PhysicsSystem2D(ECSManager& ecs) {
			}

			void init(ECSManager& ecs, Simulation2D& simulation)
			{
				m_rbManager = ecs.getComponentManager<RigidBody2D>();
				auto componentArray = m_rbManager->getComponentArray();
			}

			void update(ECSManager& ecs, Simulation2D& simulation)
			{
				auto componentArray = m_rbManager->getComponentArray();
				auto transformArray = ecs.getComponentArray<Transform>();

				for (size_t i = 0; i < componentArray->getSize(); i++)
				{
					auto& rb = componentArray->getDataAtIdx(i);
					auto& transform = transformArray->getDataFromEntity(rb.Owner);
					if (transform.isDirty)
					{
						// Override simulation transform
						simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
						transform.isDirty = false; // TODO: move somewhere else
					}

					simulation.updateTransform(transform, rb.simulationId);
				}

				auto shapeArray = ecs.getComponentArray<CustomShape>();

				for (size_t i = 0; i < shapeArray->size; i++)
				{
					auto& shape = shapeArray->getDataAtIdx(i);
					if (shape.isDirty)
					{
						simulation.updateShape(shape);
						shape.isDirty = false;
					}

				}
			}
		};
	}
}