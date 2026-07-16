#pragma once
#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-physics/Simulation2D.h"
#include "weird-physics/components/GlobalPhysicsSettings.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/components/DistanceConstraint.h"

namespace WeirdEngine
{
	namespace ECS
	{

		namespace PhysicsSystem2D
		{

			inline void init(ECSManager& ecs, Simulation2D& simulation)
			{
				ecs.registerComponent<GlobalPhysicsSettings>();
				ecs.registerComponent<Spring>();
				ecs.registerComponent<DistanceConstraint>();
			}

			inline void update(ECSManager& ecs, Simulation2D& simulation)
			{
				auto m_rbManager = ecs.getComponentManager<RigidBody2D>();
				auto componentArray = m_rbManager->getComponentArray();
				auto transformArray = ecs.getComponentArray<Transform>();

				for (size_t i = 0; i < componentArray->getSize(); i++)
				{
					Entity entity = componentArray->getEntityAtIdx(i);
					auto& rb = componentArray->getDataAtIdx(i);
					auto& transform = transformArray->getDataFromEntity(entity);
					if (transformArray->isEntityDirty(entity))
					{
						// Override simulation transform
						simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
						transformArray->setEntityDirty(entity, false); // TODO: move somewhere else
					}

					if (componentArray->isDirty(i))
					{
						simulation.setVelocity(rb.simulationId, rb.velocity);
						
						if (rb.isFixed)
							simulation.fix(rb.simulationId);
						else
							simulation.unFix(rb.simulationId);
							
						componentArray->setDirty(i, false);
					}

					if (glm::length2(rb.pendingImpulseForce) > 0.0001f)
					{
						simulation.addImpulseForce(rb.simulationId, rb.pendingImpulseForce);
						rb.pendingImpulseForce = glm::vec2(0.0f);
					}

					if (glm::length2(rb.pendingContinuousForce) > 0.0001f)
					{
						simulation.setContinuousForce(rb.simulationId, rb.pendingContinuousForce);
						rb.pendingContinuousForce = glm::vec2(0.0f);
					}

					simulation.updateTransform(transform, rb.simulationId);
				}

				auto shapeArray = ecs.getComponentArray<CustomShape>();

				for (size_t i = 0; i < shapeArray->getSize(); i++)
				{
					Entity entity = shapeArray->getEntityAtIdx(i);
					auto& shape = shapeArray->getDataAtIdx(i);
					if (shapeArray->isDirty(i))
					{
						simulation.updateShape(entity, shape);
						shapeArray->setDirty(i, false);
					}
				}

				auto globalSettingsArray = ecs.getComponentArray<GlobalPhysicsSettings>();
				if (globalSettingsArray && globalSettingsArray->getSize() > 0)
				{
					auto& settings = globalSettingsArray->getDataAtIdx(0);
					if (globalSettingsArray->isDirty(0))
					{
						simulation.setGravity(settings.gravity);
						simulation.setDamping(settings.damping);
						globalSettingsArray->setDirty(0, false);
					}
				}

				auto springArray = ecs.getComponentArray<Spring>();
				if (springArray)
				{
					for (size_t i = 0; i < springArray->getSize(); i++)
					{
						auto& spring = springArray->getDataAtIdx(i);
						if (springArray->isDirty(i) && spring.entityA != INVALID_ENTITY && spring.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(spring.entityA) && ecs.hasComponent<RigidBody2D>(spring.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(spring.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(spring.entityB).simulationId;
								simulation.addSpring(simIdA, simIdB, spring.stiffness, spring.restDistance);
								springArray->setDirty(i, false);
							}
						}
					}
				}

				auto distConstraintArray = ecs.getComponentArray<DistanceConstraint>();
				if (distConstraintArray)
				{
					for (size_t i = 0; i < distConstraintArray->getSize(); i++)
					{
						auto& constraint = distConstraintArray->getDataAtIdx(i);
						if (distConstraintArray->isDirty(i) && constraint.entityA != INVALID_ENTITY && constraint.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(constraint.entityA) && ecs.hasComponent<RigidBody2D>(constraint.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(constraint.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(constraint.entityB).simulationId;
								simulation.addPositionConstraint(simIdA, simIdB, constraint.distance);
								distConstraintArray->setDirty(i, false);
							}
						}
					}
				}

				// Dispatch all continuous force writes to the physics thread
				simulation.swapContinuousForces();

				// Activate any newly created rigidbodies now that all their
				// initialization commands (position, velocity, mass, etc.)
				// have been queued ahead of this in m_pendingCommands.
				simulation.activatePendingBodies();
			}
		} // namespace PhysicsSystem2D
	} // namespace ECS
} // namespace WeirdEngine