#pragma once
#pragma once
#include "../ECS.h"
#include "../../../weird-physics/Simulation2D.h"

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

			void init(ECSManager& ecs, Simulation2D& simulation) {

				m_rbManager = ecs.getComponentManager<RigidBody2D>();
				auto componentArray = m_rbManager->getComponentArray();


			}

			void update(ECSManager& ecs, Simulation2D& simulation) {


				auto componentArray = m_rbManager->getComponentArray();

				//for (size_t i = simulation.getSize(); i < componentArray.size; i++)
				//{
				//	RigidBody2D& rb = componentArray[i];
				//	//rb.simulationId = simulation.generateSimulationID();
				//	Transform& transform = ecs.getComponent<Transform>(rb.Owner);
				//	simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
				//}

				for (size_t i = 0; i < componentArray->getSize(); i++)
				{
					auto& rb = componentArray->getDataAtIdx(i);
					Transform& transform = ecs.getComponent<Transform>(rb.Owner);
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
					if (shape.m_isDirty)
					{
						simulation.updateShape(shape);
						shape.m_isDirty = false;
					}

				}
			}

		private:
			void addForce(ECSManager& ecs, Simulation2D& simulation, Entity entity, vec2 force)
			{
				simulation.addForce(ecs.getComponent<RigidBody2D>(entity).simulationId, force);
			}

			// This shouldn't exist. Editing the transform and setting it dirty should be enough
			void setPosition(ECSManager& ecs, Simulation2D& simulation, Entity entity, vec2 position) {
				simulation.setPosition(ecs.getComponent<RigidBody2D>(entity).simulationId, position);
			}
		};
	}
}