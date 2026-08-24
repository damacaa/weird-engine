#pragma once
#pragma once
#include "weird-engine/ecs/Registry.h"
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

			inline void init(Registry& registry, Simulation2D& simulation)
			{
				registry.registerComponent<GlobalPhysicsSettings>();
				registry.registerComponent<Spring>();
				registry.registerComponent<DistanceConstraint>();
			}

			inline void update(Registry& registry, Simulation2D& simulation)
			{
				// Pass 1: ECS -> physics. Writes are queued as commands and the
				// physics thread applies them on its next step.
				registry.forEach<RigidBody2D, Transform>(
					[&](Entity entity, RigidBody2D& rb, Transform& transform)
					{
						if (registry.isComponentDirty(transform))
						{
							// Override simulation transform
							simulation.setPosition(rb.simulationId, glm::vec2(transform.position));
							registry.setComponentDirty(transform, false); // TODO: move somewhere else
						}

						if (registry.isComponentDirty(rb))
						{
							simulation.setVelocity(rb.simulationId, rb.velocity);

							if (rb.isFixed)
								simulation.fix(rb.simulationId);
							else
								simulation.unFix(rb.simulationId);

							registry.setComponentDirty(rb, false);
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
					});

				registry.forEach<CustomShape>(
					[&](Entity entity, CustomShape& shape)
					{
						if (registry.isComponentDirty(shape))
						{
							simulation.updateShape(entity, shape);
							registry.setComponentDirty(shape, false);
						}
					});

				registry.forEach<GlobalPhysicsSettings>(
					[&](Entity entity, GlobalPhysicsSettings& settings)
					{
						if (registry.isComponentDirty(settings))
						{
							simulation.setGravity(settings.gravity);
							simulation.setDamping(settings.damping);
							registry.setComponentDirty(settings, false);
						}
					});

				registry.forEach<Spring>(
					[&](Entity entity, Spring& spring)
					{
						if (registry.isComponentDirty(spring) && spring.entityA != INVALID_ENTITY &&
							spring.entityB != INVALID_ENTITY)
						{
							if (registry.hasComponent<RigidBody2D>(spring.entityA) &&
								registry.hasComponent<RigidBody2D>(spring.entityB))
							{
								auto simIdA = registry.getComponent<RigidBody2D>(spring.entityA).simulationId;
								auto simIdB = registry.getComponent<RigidBody2D>(spring.entityB).simulationId;
								if (!simulation.setDistanceConstraintDistance(simIdA, simIdB, spring.restDistance))
								{
									simulation.addSpring(simIdA, simIdB, spring.stiffness, spring.restDistance);
								}
								registry.setComponentDirty(spring, false);
							}
						}
					});

				registry.forEach<DistanceConstraint>(
					[&](Entity entity, DistanceConstraint& constraint)
					{
						if (registry.isComponentDirty(constraint) && constraint.entityA != INVALID_ENTITY &&
							constraint.entityB != INVALID_ENTITY)
						{
							if (registry.hasComponent<RigidBody2D>(constraint.entityA) &&
								registry.hasComponent<RigidBody2D>(constraint.entityB))
							{
								auto simIdA = registry.getComponent<RigidBody2D>(constraint.entityA).simulationId;
								auto simIdB = registry.getComponent<RigidBody2D>(constraint.entityB).simulationId;
								if (!simulation.setDistanceConstraintDistance(simIdA, simIdB, constraint.distance))
								{
									simulation.addPositionConstraint(simIdA, simIdB, constraint.distance);
								}
								registry.setComponentDirty(constraint, false);
							}
						}
					});

				// Dispatch all continuous force writes to the physics thread
				simulation.swapContinuousForces();

				// Activate any newly created rigidbodies now that all their
				// initialization commands (position, velocity, mass, etc.)
				// have been queued ahead of this in m_pendingCommands.
				simulation.activatePendingBodies();

				// Pass 2: physics -> ECS readback. The published buffers are
				// copied into a reusable snapshot under a short lock; the ECS
				// iteration then runs without holding the simulation mutex, so
				// the physics thread can keep stepping meanwhile.
				static Simulation2D::ReadBufferSnapshot readSnapshot;

				simulation.copyReadBuffers(readSnapshot);

				registry.forEach<RigidBody2D, Transform>(
					[&](Entity entity, RigidBody2D& rb, Transform& transform)
					{
						vec2 position = readSnapshot.positions[rb.simulationId];
						transform.position.x = position.x;
						transform.position.y = position.y;

						rb.velocity = readSnapshot.velocities[rb.simulationId];
					});
			}
		} // namespace PhysicsSystem2D
	} // namespace ECS
} // namespace WeirdEngine