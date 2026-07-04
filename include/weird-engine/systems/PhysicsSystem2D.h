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
					auto& rb = componentArray->getDataAtIdx(i);
					auto& transform = transformArray->getDataFromEntity(rb.Owner);
					if (transform.isDirty)
					{
						// Override simulation transform
						simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
						transform.isDirty = false; // TODO: move somewhere else
					}

					if (rb.isVelocityDirty)
					{
						simulation.setVelocity(rb.simulationId, rb.velocity);
						rb.isVelocityDirty = false;
					}

					if (glm::length2(rb.pendingForce) > 0.0001f)
					{
						simulation.addForce(rb.simulationId, rb.pendingForce);
						rb.pendingForce = glm::vec2(0.0f);
					}

					if (rb.isFixedDirty)
					{
						if (rb.isFixed)
							simulation.fix(rb.simulationId);
						else
							simulation.unFix(rb.simulationId);
						rb.isFixedDirty = false;
					}

					simulation.updateTransform(transform, rb.simulationId);
				}

				auto shapeArray = ecs.getComponentArray<CustomShape>();

				for (size_t i = 0; i < shapeArray->getSize(); i++)
				{
					auto& shape = shapeArray->getDataAtIdx(i);
					if (shape.isDirty)
					{
						simulation.updateShape(shape);
						shape.isDirty = false;
					}
				}

				auto globalSettingsArray = ecs.getComponentArray<GlobalPhysicsSettings>();
				if (globalSettingsArray && globalSettingsArray->getSize() > 0)
				{
					auto& settings = globalSettingsArray->getDataAtIdx(0);
					if (settings.isGravityDirty)
					{
						simulation.setGravity(settings.gravity);
						settings.isGravityDirty = false;
					}
					if (settings.isDampingDirty)
					{
						simulation.setDamping(settings.damping);
						settings.isDampingDirty = false;
					}
				}

				auto springArray = ecs.getComponentArray<Spring>();
				if (springArray)
				{
					for (size_t i = 0; i < springArray->getSize(); i++)
					{
						auto& spring = springArray->getDataAtIdx(i);
						if (spring.isDirty && spring.entityA != INVALID_ENTITY && spring.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(spring.entityA) && ecs.hasComponent<RigidBody2D>(spring.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(spring.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(spring.entityB).simulationId;
								simulation.addSpring(simIdA, simIdB, spring.stiffness, spring.restDistance);
								spring.isDirty = false;
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
						if (constraint.isDirty && constraint.entityA != INVALID_ENTITY && constraint.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(constraint.entityA) && ecs.hasComponent<RigidBody2D>(constraint.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(constraint.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(constraint.entityB).simulationId;
								simulation.addPositionConstraint(simIdA, simIdB, constraint.distance);
								constraint.isDirty = false;
							}
						}
					}
				}
			}
		} // namespace PhysicsSystem2D
	} // namespace ECS
} // namespace WeirdEngine