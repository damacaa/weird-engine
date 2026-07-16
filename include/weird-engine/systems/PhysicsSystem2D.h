#pragma once
#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-physics/components/DistanceConstraint.h"
#include "weird-physics/components/GlobalPhysicsSettings.h"
#include "weird-physics/components/Spring.h"
#include "weird-physics/Simulation2D.h"

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
				ecs.forEach<RigidBody2D, Transform>(
					[&](Entity entity, RigidBody2D& rb, Transform& transform)
					{
						if (ecs.isComponentDirty(transform))
						{
							// Override simulation transform
							simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
							ecs.setComponentDirty(transform, false); // TODO: move somewhere else
						}

						if (ecs.isComponentDirty(rb))
						{
							simulation.setVelocity(rb.simulationId, rb.velocity);

							if (rb.isFixed)
								simulation.fix(rb.simulationId);
							else
								simulation.unFix(rb.simulationId);

							ecs.setComponentDirty(rb, false);
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
					});

				ecs.forEach<CustomShape>(
					[&](Entity entity, CustomShape& shape)
					{
						if (ecs.isComponentDirty(shape))
						{
							simulation.updateShape(entity, shape);
							ecs.setComponentDirty(shape, false);
						}
					});

				ecs.forEach<GlobalPhysicsSettings>(
					[&](Entity entity, GlobalPhysicsSettings& settings)
					{
						if (ecs.isComponentDirty(settings))
						{
							simulation.setGravity(settings.gravity);
							simulation.setDamping(settings.damping);
							ecs.setComponentDirty(settings, false);
						}
					});

				ecs.forEach<Spring>(
					[&](Entity entity, Spring& spring)
					{
						if (ecs.isComponentDirty(spring) && spring.entityA != INVALID_ENTITY &&
							spring.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(spring.entityA) &&
								ecs.hasComponent<RigidBody2D>(spring.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(spring.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(spring.entityB).simulationId;
								simulation.addSpring(simIdA, simIdB, spring.stiffness, spring.restDistance);
								ecs.setComponentDirty(spring, false);
							}
						}
					});

				ecs.forEach<DistanceConstraint>(
					[&](Entity entity, DistanceConstraint& constraint)
					{
						if (ecs.isComponentDirty(constraint) && constraint.entityA != INVALID_ENTITY &&
							constraint.entityB != INVALID_ENTITY)
						{
							if (ecs.hasComponent<RigidBody2D>(constraint.entityA) &&
								ecs.hasComponent<RigidBody2D>(constraint.entityB))
							{
								auto simIdA = ecs.getComponent<RigidBody2D>(constraint.entityA).simulationId;
								auto simIdB = ecs.getComponent<RigidBody2D>(constraint.entityB).simulationId;
								simulation.addPositionConstraint(simIdA, simIdB, constraint.distance);
								ecs.setComponentDirty(constraint, false);
							}
						}
					});

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