#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/math/SDF.h"
#include "weird-engine/vec.h"
#include "weird-physics/BodyUserData.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-physics/Simulation2D.h"

namespace WeirdEngine
{
	// Result of a scene query (PhysicsService::sampleAt / PhysicsService::raymarch).
	// distance: sampleAt = signed distance from the query point to the closest
	// surface (negative = inside a shape); the entity can be valid even when the
	// point is far from it, so check the distance. raymarch = distance traveled
	// from the origin to the first hit, or maxDistance when nothing was hit.
	// entity: owner of the closest/hit surface, or INVALID_ENTITY when no shape or
	// rigidbody was found (raymarch miss / empty scene).
	struct RaymarchResult
	{
		float distance;
		Entity entity;
	};

	class PhysicsService
	{
	public:
		PhysicsService(Registry& registry, Simulation2D& simulation,
					   std::vector<std::shared_ptr<IMathExpression>>& sdfs)
			: m_registry(registry)
			, m_simulation(simulation)
			, m_sdfs(sdfs)
		{
		}

		void setGravity(float gravity)
		{
			m_simulation.setGravity(gravity);
		}

		float getGravity() const
		{
			return m_simulation.getGravity();
		}

		void setDamping(float damping)
		{
			m_simulation.setDamping(damping);
		}

		float getDamping() const
		{
			return m_simulation.getDamping();
		}

		void setFriction(float friction)
		{
			m_simulation.setFriction(friction);
		}

		float getFriction() const
		{
			return m_simulation.getFriction();
		}

		void pause()
		{
			m_simulation.pause();
		}

		void resume()
		{
			m_simulation.resume();
		}

		bool isPaused() const
		{
			return m_simulation.isPaused();
		}

		Entity entityForSimulationId(SimulationID simulationId) const
		{
			auto rigidBodies = m_registry.getComponentArray<RigidBody2D>();
			if (simulationId >= static_cast<SimulationID>(rigidBodies->getSize()))
				return INVALID_ENTITY;

			return rigidBodies->getEntityAtIdx(static_cast<size_t>(simulationId));
		}

		// Hit-test active rigidbodies at a 2D world position. Returns the Entity owning
		// the rigidbody at that position, or INVALID_ENTITY if no rigidbody was hit.
		Entity getRigidbodyAt(vec2 pos) const
		{
			SimulationID simId = m_simulation.raycast(pos);
			if (simId == INVALID_SIMULATION_ID)
				return INVALID_ENTITY;

			return entityForSimulationId(simId);
		}

		// Per-body user data. Set the data right after adding the RigidBody2D
		// component (read rb.simulationId from it). Ownership is transferred to
		// the simulation (e.g. std::make_unique<CharacterData>()): it deletes
		// the data when the body is removed or when the simulation is
		// destroyed. Do not retain the pointer after the call; query it back
		// through getUserData()/getUserDataAs<T>().
		void setUserData(SimulationID id, std::unique_ptr<BodyUserData> data)
		{
			m_simulation.setUserData(id, std::move(data));
		}

		BodyUserData* getUserData(SimulationID id)
		{
			return m_simulation.getUserData(id);
		}

		template <typename T> T* getUserDataAs(SimulationID id)
		{
			return m_simulation.getUserDataAs<T>(id);
		}

		template <typename Fn> void forEachUserData(Fn&& fn)
		{
			m_simulation.forEachUserData(std::forward<Fn>(fn));
		}

		// Point query: evaluate the scene at a world position. Returns the entity
		// owning the closest surface and the signed distance to it (negative =
		// inside a shape). Rigidbodies count as circles; pass false to test world
		// shapes only. Main thread only.
		RaymarchResult sampleAt(vec2 point, bool includeRigidbodies = true);

		// Sample signed distance from the SDF at a world position (negative = inside a shape).
		// Pass false to test world shapes only (excluding rigidbodies). Main thread only.
		float sampleDistance(vec2 point, bool includeRigidbodies = true);

		// Ray query: march from origin along direction up to maxDistance. Returns
		// the distance traveled to the first surface hit and the owning entity, or
		// {maxDistance, INVALID_ENTITY} when the ray misses. Main thread only.
		RaymarchResult raymarch(glm::vec2 origin, glm::vec2 direction, float epsilon = 0.001f,
								float maxDistance = 150.0f, bool includeRigidbodies = true);

		// Evaluates the normalized unit gradient of the signed distance field (∇d) at a world position.
		// Points in the direction of steepest distance increase (directly away from nearest geometry).
		// Corresponds to the outward surface normal when sampled on or near a shape boundary.
		// Pass false to test world shapes only (excluding rigidbodies). Main thread only.
		glm::vec2 sampleGradient(vec2 point, bool includeRigidbodies = true, float epsilon = 0.005f);

	private:
		Registry& m_registry;
		Simulation2D& m_simulation;
		std::vector<std::shared_ptr<IMathExpression>>& m_sdfs;
	};
} // namespace WeirdEngine
