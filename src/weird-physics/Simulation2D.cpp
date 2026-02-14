#include "weird-physics/Simulation2D.h"

#include <algorithm>

#include "glm/gtx/norm.hpp"

namespace WeirdEngine
{

#define MEASURE_PERFORMANCE false
#define INTEGRATION_METHOD 1

	using namespace std::chrono;

	constexpr float SIMULATION_FREQUENCY = 250;
	constexpr double FIXED_DELTA_TIME = 1.f / SIMULATION_FREQUENCY;
	constexpr float FIXED_DELTA_TIME_F = FIXED_DELTA_TIME;

	constexpr size_t MAX_STEPS = 10;

	const float EPSILON = 0.0001f;



	Simulation2D::Simulation2D(size_t size) : m_isPaused(false),
		m_positions(new vec2[size]),
		m_positionsRead(new vec2[size]),
		m_positionsAux(new vec2[size]),
		m_previousPositions(new vec2[size]),
		m_velocities(new vec2[size]),
		m_forces(new vec2[size]),
		m_externalForcesSinceLastUpdate(false),
		m_externalForces(new vec2[size]),
		m_mass(new float[size]),
		m_invMass(new float[size]),
		m_maxSize(size),
		m_size(0),
		m_lastIdGiven(-1),
		m_simulationDelay(0),
		m_simulationTime(0),
		m_substeps(1),
		m_gravity(-10),
		m_push(10.0f * SIMULATION_FREQUENCY),
		m_damping(0.001f),
		m_simulating(false),
		m_collisionDetectionMethod(MethodNaive),
		m_useSimdOperations(false),
		m_diameter(1.0f),
		m_diameterSquared(m_diameter* m_diameter),
		m_radious(m_diameter / 2.0f),
		m_collisionMap(size)
	{
		for (size_t i = 0; i < m_maxSize; i++)
		{
			m_positions[i] = vec2(0.0f, 0.0f);
			m_positionsRead[i] = vec2(0.0f, 0.0f);
			m_previousPositions[i] = vec2(0.0f, 0.0f);

			m_velocities[i] = vec2(0.0f);
			m_forces[i] = vec2(0.0f);

			m_mass[i] = 1000.0f;
			m_invMass[i] = 0.001f;
		}

		m_sdfs = std::make_shared<std::vector<std::shared_ptr<IMathExpression>>>();
	}

	Simulation2D::~Simulation2D()
	{
		delete[] m_positions;
		delete[] m_positionsRead;
		delete[] m_positionsAux;
		delete[] m_previousPositions;
		delete[] m_velocities;
		delete[] m_forces;
		delete[] m_mass;
		delete[] m_invMass;
	}

	void Simulation2D::pause()
	{
		m_isPaused = true;
	}

	void Simulation2D::resume()
	{
		m_isPaused = false;
	}

	bool Simulation2D::isPaused()
	{
		return m_isPaused;
	}

#pragma region PhysicsUpdate

	void Simulation2D::update(double delta)
	{
		{
			m_simulationDelay += delta;
		}

		if (m_simulating)
			return;

		process();
	}

#if MEASURE_PERFORMANCE
	double g_time = 0;
	uint32_t g_simulationSteps = 0;
	uint64_t g_collisionCount = 0;
#endif

	void Simulation2D::process()
	{
		int steps = 0;

		while (m_simulationDelay >= FIXED_DELTA_TIME && steps < MAX_STEPS)
		{
#if MEASURE_PERFORMANCE
			auto start = std::chrono::high_resolution_clock::now();
#endif

			if (!m_isPaused)
			{
				checkCollisions();
				applyForces();
				solveConstraints();
				step((float)FIXED_DELTA_TIME);
				++steps;
				{
					// m_simulationTime += FIXED_DELTA_TIME;
					m_simulationTime.fetch_add(FIXED_DELTA_TIME);

					// Notify collision callback
					if (m_stepCallback)
					{
						m_stepCallback(m_callbackUserData);
					}
				}
			}

			{
				// std::lock_guard<std::mutex> lock(g_simulationTimeMutex); // Lock the mutex
				m_simulationDelay -= FIXED_DELTA_TIME;
			}



#if MEASURE_PERFORMANCE
			// Get the ending time
			auto end = std::chrono::high_resolution_clock::now();

			// Calculate the duration
			std::chrono::duration<double> duration = end - start;

			g_simulationSteps++;
			g_time += 1000 * duration.count();

			if (g_simulationSteps == 20 * SIMULATION_FREQUENCY)
			{
				auto average = g_time / g_simulationSteps;
				std::cout << average << "ms" << std::endl;
				std::cout << g_collisionCount << " collisions" << std::endl;
			}
#endif
		}
	}

	double Simulation2D::getSimulationTime()
	{
		// std::lock_guard<std::mutex> lock(g_simulationTimeMutex);
		return m_simulationTime;
	}

	void Simulation2D::startSimulationThread()
	{
		m_simulating = true;

		m_simulationThread = std::thread(&Simulation2D::runSimulationThread, this);
	}

	void Simulation2D::stopSimulationThread()
	{
		if (!m_simulating)
			return;

		m_simulating = false;
		m_simulationThread.join();
	}

	void Simulation2D::checkCollisions()
	{
		// Detect collisions
#if MEASURE_PERFORMANCE
		int checks = 0;
#endif
		m_collisions.clear();

		for (size_t i = 0; i < m_size; i++)
		{
			// Simple collisions
			for (size_t j = i + 1; j < m_size; j++)
			{
				vec2 ij = m_positions[j] - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared < m_diameterSquared)
				{
					m_collisions.emplace_back(Collision(i, j, ij));
				}

#if MEASURE_PERFORMANCE
				checks++;
#endif
			}
		}

#if MEASURE_PERFORMANCE
		if (g_simulationSteps == 0)
			std::cout << "First frame checks: " << checks << std::endl;
#endif

		// Check shape collisions
		for (size_t i = 0; i < m_size; i++)
		{
			vec2& p = m_positions[i];

			// Check
			bool currentCollision = false;
			ShapeCollisionEvent collisionEvent;
			collisionEvent.body = i;

			// Static shapes
			int shapeIdx;
			float d = map(p, shapeIdx);
			collisionEvent.shape = shapeIdx;

			if (d < m_radious)
			{
				// Collision normal calculation
				float d1 = map(p + vec2(EPSILON, 0.0)) - map(p - vec2(EPSILON, 0.0));
				float d2 = map(p + vec2(0.0, EPSILON)) - map(p - vec2(0.0, EPSILON));

				// float d1 = d - map(vec2(p.x - EPSILON, p.y));
				// float d2 = d - map(vec2(p.x, p.y - EPSILON));

				collisionEvent.normal = normalize(vec2(d1, d2));

				float distanceAtSurface = map(p - ((m_radious) * collisionEvent.normal));
				if (distanceAtSurface <= 0.0f) // Bad solution? Check if the distance at approximate contact point is small enough
				{
					float penetration = (std::min)(-distanceAtSurface, m_radious - d);
					currentCollision = true;

					collisionEvent.penetration = penetration;
					collisionEvent.position = p - (0.5f * collisionEvent.normal);
					collisionEvent.velocity = m_velocities[i];

					constexpr float DYNAMIC_FRICTION = 0.1f;
					collisionEvent.friction = DYNAMIC_FRICTION;

					constexpr float ABSORTION = 10.0f;
					collisionEvent.absortion = ABSORTION;
				}
			}

			bool previousCollision = m_collisionMap[i];
			if (currentCollision != previousCollision)
			{
				if (currentCollision)
				{
					// Inform of new collision
					collisionEvent.state = CollisionState::START;
					m_collisionQueue.push_back(collisionEvent);
				}
				else
				{
					// Inform of end of collision
					collisionEvent.state = CollisionState::END;
					m_collisionQueue.push_back(collisionEvent);
				}

				m_collisionMap[i] = currentCollision;
			}
			else if (currentCollision)
			{
				collisionEvent.state = CollisionState::CONTINUE;
				m_collisionQueue.push_back(collisionEvent);
			}
		}
	}

	void Simulation2D::solveCollisionsPositionBased()
	{
		// Calculate forces
		for (auto it = m_collisions.begin(); it != m_collisions.end(); ++it)
		{
			Collision col = *it;
			vec2 penetration = 0.5f * ((m_radious + m_radious) - length(col.AB)) * normalize(col.AB);

			m_positions[col.A] -= penetration;
			m_positions[col.B] += penetration;
		}
	}

	static float fOpUnionSoft(float a, float b, float r)
	{
		r *= 1.0f; // 4.0f orignal
		float h = std::max(r - abs(a - b), 0.0f);
		return std::min(a, b) - h * h * 0.25f / r;
	}

	// Smooth subtraction (a - b), 2D SDF
	static float fOpSubSoft(float a, float b, float r)
	{
		return -fOpUnionSoft(b, -a, r);
	}

	float Simulation2D::map(vec2 p)
	{
		int dummy;
		return map(p, dummy);
	}

	float Simulation2D::map(vec2 p, int& closestShape)
	{
		closestShape = 0;

		float d = 1000.0f;
		float minD = d;
		float currentGroupMinDistance = 1000.0f;

		auto currentGroup = 0;

		for (int i = 0; i < m_objects.size(); i++)
		{


			DistanceFieldObject2D& obj = m_objects[i];
			if (obj.distanceFieldId >= m_sdfs->size())
			{
				continue;
			}

			obj.parameters[8] = m_simulationTime;
			obj.parameters[9] = p.x;
			obj.parameters[10] = p.y;

			if (obj.groupId != currentGroup)
			{
				currentGroup = obj.groupId;
				d = std::min(d, currentGroupMinDistance);
				currentGroupMinDistance = 100000.0f;
			}

			// Distance
			(*m_sdfs)[obj.distanceFieldId]->propagateValues(obj.parameters);

			float dist = (*m_sdfs)[obj.distanceFieldId]->getValue();

			bool globalEffect = obj.groupId == CustomShape::GLOBAL_GROUP;

			float currentMinDistance = globalEffect ? d : currentGroupMinDistance;

			// Combination
			switch (obj.combinationId) {
				case CombinationType::Addition: {
					currentMinDistance = std::min(currentMinDistance, dist);
					break;
				}
				case CombinationType::Subtraction: {
					currentMinDistance = std::max(currentMinDistance, -dist);
					break;
				}
				case CombinationType::Intersection: {
					currentMinDistance = std::max(currentMinDistance, dist);
					break;
				}
				case CombinationType::SmoothAddition: {
					currentMinDistance = fOpUnionSoft(currentMinDistance, dist, 1.0f);
					break;
				}
				case CombinationType::SmoothSubtraction: {
					currentMinDistance = fOpSubSoft(currentMinDistance, dist, 1.0f);
					break;
				}
				default:
					break;
			}

			if (currentMinDistance < minD)
			{
				minD = currentMinDistance;
				closestShape = i;
			}

			if (globalEffect) {
				d = currentMinDistance;
			} else {
				currentGroupMinDistance = currentMinDistance;
			}
		}

		d = std::min(d, currentGroupMinDistance);

		return d - 0.05f;
	}


	void Simulation2D::applyForces()
	{
		// External forces
		{
			std::lock_guard<std::mutex> lock(m_externalForcesMutex);
			if (m_externalForcesSinceLastUpdate && m_size - 1 == m_lastIdGiven)
			{
				m_externalForcesSinceLastUpdate = false;
				for (size_t i = 0; i < m_size; i++)
				{
					vec2& f = m_externalForces[i];
					m_forces[i] = f;
					m_externalForces[i] = vec2(0);
				}
			}
		}

		// Sphere collisions
		for (auto it = m_collisions.begin(); it != m_collisions.end(); ++it)
		{
			Collision col = *it;

			float lengthSquared = glm::length2(col.AB);
			vec2 normal = lengthSquared > 0.0f ? normalize(col.AB) : vec2(1.0f);
			float penetration = (m_radious + m_radious) - length(col.AB);

			// Position
			/*vec2 translation = 0.5f * penetration * normal;
			m_positions[col.A] -= translation;
			m_positions[col.B] += translation;*/

			// Impulse method
			float restitution = 0.5f;
			vec2 vRel = m_velocities[col.B] - m_velocities[col.A];
			float velocityAlongNormal = glm::dot(normal, vRel);
			float impulseMagnitude = -(1 + restitution) * velocityAlongNormal / (m_invMass[col.A] + m_invMass[col.B]);
			vec2 impulse = impulseMagnitude * normal;

			m_velocities[col.A] -= m_invMass[col.A] * impulse;
			m_velocities[col.B] += m_invMass[col.B] * impulse;

			// Penalty method
			vec2 penalty = m_push * penetration * normal;
			m_forces[col.A] -= m_mass[col.A] * penalty;
			m_forces[col.B] += m_mass[col.B] * penalty;

			// Notify collision callback
			if (m_collisionCallback)
			{
				CollisionEvent event{ col.A, col.B };
				m_collisionCallback(event, m_callbackUserData);
			}

#if MEASURE_PERFORMANCE
			g_collisionCount++;
#endif
		}

		// Shape collisions
		for (auto &collisionEvent: m_collisionQueue)
		{
			// Send event
			if (m_shapeCollisionCallback)
			{
				m_shapeCollisionCallback(collisionEvent, m_callbackUserData); // Scene can modify values
			}

			if (collisionEvent.state == CollisionState::END)
				continue;

			vec2 vel = collisionEvent.velocity;
			float speed = length(vel);

			vec2 velocityDirection = speed > 0.0f ? vel / speed : vec2(0.0f); // Avoid NaN
			float velocityAlongNormal = glm::dot(velocityDirection, collisionEvent.normal);

			// Apply friction
			float friction = collisionEvent.friction * (1.0f - abs(velocityAlongNormal)) * (speed + 1.1f);
			m_velocities[collisionEvent.body] -= FIXED_DELTA_TIME_F * friction * velocityDirection; // Lose velocity on collision

			// Absortion
			float absortionRate = std::max(0.0f, collisionEvent.absortion * velocityAlongNormal);
			m_velocities[collisionEvent.body] -= FIXED_DELTA_TIME_F * absortionRate * speed * collisionEvent.normal;

			// Penalty
			vec2 v = collisionEvent.penetration * collisionEvent.penetration * collisionEvent.normal;
			vec2 force = m_mass[collisionEvent.body] * m_push * v;

			m_forces[collisionEvent.body] += force;
		}

		m_collisionQueue.clear();


		// Apply extra forces
		for (size_t i = 0; i < m_size; i++)
		{
			vec2& p = m_positions[i];
			// vec2& force = m_forces[i];

			// Gravity
			if (!m_attracttionEnabled)
			{
				m_forces[i].y += m_mass[i] * m_gravity;
			}
		}
	}

	void Simulation2D::solveConstraints()
	{
		for (auto it = m_springs.begin(); it != m_springs.end(); ++it)
		{
			Spring spring = *it;

			vec2 v = m_positions[spring.B] - m_positions[spring.A];
			vec2 n = normalize(v);
			float distance = length(v);
			float d = spring.Distance - distance;
			// d = std::clamp(d, -10.0f, 10.0f);

			vec2 springForce = 0.5f * spring.K * d * n;
			vec2 damping = spring.Damping * (m_velocities[spring.B] - m_velocities[spring.A]);

			vec2 f = springForce - damping;

			m_forces[spring.A] -= f;
			m_forces[spring.B] += f;
		}

		for (auto it = m_distanceConstraints.begin(); it != m_distanceConstraints.end(); ++it)
		{
			DistanceConstraint spring = *it;

			vec2 v = m_positions[spring.B] - m_positions[spring.A];
			vec2 n = normalize(v);
			float distance = length(v);

			float correction = (distance - spring.Distance);
			vec2 correctionVector = -correction * 0.5f * n;

			vec2 pA = m_positions[spring.A] - correctionVector;
			vec2 pB = m_positions[spring.B] + correctionVector;

			m_positions[spring.A] = pA;
			m_positions[spring.B] = pB;
		}

		for (auto it = m_gravitationalConstraints.begin(); it != m_gravitationalConstraints.end(); ++it)
		{
			GravitationalConstraint constraint = *it;

			vec2 v = m_positions[constraint.B] - m_positions[constraint.A];
			vec2 n = normalize(v);
			float distance = length(v);

			float f = constraint.g * (m_mass[constraint.A] * m_mass[constraint.B]) / (distance * distance);

			m_forces[constraint.A] += f * n;
			m_forces[constraint.B] -= f * n;
		}

		{
			std::lock_guard<std::mutex> lock(m_fixMutex);
			for (auto it = m_fixedObjects.begin(); it != m_fixedObjects.end(); ++it)
			{
				SimulationID id = *it;

				m_positions[id] = m_previousPositions[id];
				m_velocities[id] = vec2(0.0f);
				m_forces[id] = vec2(0.0f);
			}
		}
	}

	void Simulation2D::step(const float timeStep)
	{
#if INTEGRATION_METHOD == 0

		// Integrate with euler
		for (size_t i = 0; i < m_size; i++)
		{
			vec2 acc = m_forces[i] * m_invMass[i];
			m_velocities[i] += timeStep * (acc - (m_damping * m_velocities[i]));
			m_positions[i] += timeStep * m_velocities[i];

			// Reset forces
			m_forces[i] = vec2(0.0f);
		}

#elif INTEGRATION_METHOD == 1

		// Integrate with verlet
		float invTimeStep = 1.0f / timeStep;
		for (size_t i = 0; i < m_size; i++)
		{
			// Acceleration
			vec2 acc = m_forces[i] * m_invMass[i] * timeStep * timeStep;

			// Store the current position
			vec2 currentPosition = m_positions[i];

			// Update the position using Verlet integration
			vec2 newPosition = m_positions[i] + (m_velocities[i]) * timeStep * (1.0f - m_damping) + (acc);
			m_positions[i] = newPosition;

			// Update the previous position
			m_previousPositions[i] = currentPosition;

			// Reset forces
			m_forces[i] = vec2(0.0f);

			vec2 newVelocity = (newPosition - currentPosition) * invTimeStep;
			m_velocities[i] = newVelocity;
		}

#endif

		// TODO: improve this shit
		for (size_t i = 0; i < m_size; i++)
		{
			m_positionsAux[i] = m_positions[i];
		}

		// swap buffers
		vec2* aux = m_positionsAux;
		m_positionsAux = m_positionsRead;
		m_positionsRead = aux;
	}

#pragma endregion

	SimulationID Simulation2D::generateSimulationID()
	{
		// std::cout << (m_lastIdGiven + 1) << std::endl;
		return ++m_lastIdGiven;
	}

	void Simulation2D::removeObject(SimulationID id)
	{
		auto toId = id;
		auto fromId = m_lastIdGiven;

		m_positions[toId] = m_positions[fromId];
		m_positions[toId] = m_positions[fromId];
		m_previousPositions[toId] = m_previousPositions[fromId];
		m_velocities[toId] = m_velocities[fromId];
		m_mass[toId] = m_mass[fromId];
		m_invMass[toId] = m_invMass[fromId];
		m_forces[toId] = m_forces[fromId];
		m_externalForces[toId] = m_externalForces[fromId];

		// Fix constraints (potentially slow...)

		// Remove constraints that affect deleted object
		auto RemoveByID = [toId](auto& container)
			{
				container.erase(
					std::remove_if(container.begin(), container.end(), [toId](const auto& constraint)
						{
							if (constraint.A == toId || constraint.B == toId)
							{
								return true;
							}
							return false; }),
					container.end());
			};

		RemoveByID(m_springs);
		RemoveByID(m_distanceConstraints);
		RemoveByID(m_gravitationalConstraints);

		// Remove all occurrences deleted id
		m_fixedObjects.erase(std::remove(m_fixedObjects.begin(), m_fixedObjects.end(), toId), m_fixedObjects.end());

		// If a constraint targeted the moved object, change it to its new id
		auto ChangeByID = [fromId, toId](auto& container)
			{
				for (auto& constraint : container)
				{
					if (constraint.A == fromId)
					{
						// Extra code goes here for when A is updated.
						// For example, logging or updating related state.
						constraint.A = toId;
					}
					if (constraint.B == fromId)
					{
						// Extra code goes here for when B is updated.
						// For example, logging or updating related state.
						constraint.B = toId;
					}
				}
			};

		ChangeByID(m_springs);
		ChangeByID(m_distanceConstraints);
		ChangeByID(m_gravitationalConstraints);

		// Find if moved object is fixed
		auto it = std::find(m_fixedObjects.begin(), m_fixedObjects.end(), fromId);

		// If it is, fix it at its new id
		if (it != m_fixedObjects.end())
		{
			// Compute the index by subtracting the beginning iterator
			int index = it - m_fixedObjects.begin();
			m_fixedObjects[index] = toId;
		}

		// Adjust size
		m_lastIdGiven--;
		m_size--;
	}

	size_t Simulation2D::getSize()
	{
		return m_size;
	}

	void Simulation2D::addForce(SimulationID id, const vec2& force)
	{
		std::lock_guard<std::mutex> lock(m_externalForcesMutex);

		// TODO: create two buffers (continuous forces and impulses), depending on type multiply by mass imitating 4 unity types

		m_externalForcesSinceLastUpdate = true;
		m_externalForces[id] += SIMULATION_FREQUENCY * m_mass[id] * force;
	}

	void Simulation2D::addSpring(SimulationID a, SimulationID b, float stiffness, float distance, float damping)
	{
		if (a == b)
			return;

		m_springs.emplace_back(a, b, stiffness, distance, damping);
	}

	void Simulation2D::addPositionConstraint(SimulationID a, SimulationID b, float distance)
	{
		if (a == b)
			return;

		m_distanceConstraints.emplace_back(a, b, distance);
	}

	void Simulation2D::addGravitationalConstraint(SimulationID a, SimulationID b, float gravity)
	{
		if (a == b)
			return;

		m_gravitationalConstraints.emplace_back(a, b, gravity);
	}

	void Simulation2D::fix(SimulationID id)
	{
		std::lock_guard<std::mutex> lock(m_fixMutex);
		m_fixedObjects.emplace_back(id);
	}

	void Simulation2D::unFix(SimulationID id)
	{
		std::lock_guard<std::mutex> lock(m_fixMutex);
		m_fixedObjects.erase(std::remove(m_fixedObjects.begin(), m_fixedObjects.end(), id), m_fixedObjects.end());
	}

	vec2 Simulation2D::getPosition(SimulationID entity)
	{
		return m_positionsRead[entity];
	}

	void Simulation2D::setPosition(SimulationID id, vec2 pos)
	{
		m_positions[id] = pos;
		m_positionsRead[id] = pos;
		m_previousPositions[id] = pos;
		m_velocities[id] = vec2(0.0f);
		// m_forces[entity] = vec2(0.0f);
		m_size = std::max((uint32_t)m_size, id + 1);
	}

	void Simulation2D::updateTransform(Transform& transform, SimulationID id)
	{
		transform.position.x = m_positionsRead[id].x;
		transform.position.y = m_positionsRead[id].y;
	}

	void Simulation2D::setMass(SimulationID id, float mass)
	{
		m_mass[id] = mass;
		m_invMass[id] = 1.0f / mass;
	}

	void Simulation2D::setSDFs(std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		m_sdfs = std::make_shared<std::vector<std::shared_ptr<IMathExpression>>>(sdfs);
	}

	void Simulation2D::updateShape(CustomShape& shape)
	{
		if (!shape.hasCollisions)
			return;

		DistanceFieldObject2D sdf(shape.Owner, shape.distanceFieldId, shape.combination, shape.groupIdx, shape.parameters);

		// Check if the key exists
		auto it = m_entityToObjectsIdx.find(shape.Owner);
		if (it != m_entityToObjectsIdx.end())
		{
			// Key exists, get the value
			auto id = it->second;
			m_objects[id] = sdf;
		}
		else
		{
			// Key does not exist
			m_objects.push_back(sdf);
			ShapeId id = m_objects.size() - 1;
			m_entityToObjectsIdx[shape.Owner] = id;
			shape.simulationId = id;
		}
	}

	void Simulation2D::removeShape(CustomShape& shape)
	{
		if (m_objects.size() == 0)
		{
			return;
		}

		auto it = m_entityToObjectsIdx.find(shape.Owner);
		if (it == m_entityToObjectsIdx.end())
		{
			return;
		}

		// Get delete idx
		auto idx = it->second;

		// Replace the element at idx with the last element
		m_objects[idx] = m_objects.back();

		// Point last objects entity to the new vector position
		m_entityToObjectsIdx[m_objects[idx].owner] = idx;

		// Remove the last element
		m_objects.pop_back();

		// Remove from map
		m_entityToObjectsIdx.erase(shape.Owner);
	}

	SimulationID Simulation2D::raycast(vec2 pos)
	{
			for (size_t i = 0; i < m_size; i++)
			{
				vec2 ij = pos - m_positions[i];

				float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

				if (distanceSquared < m_radious * m_radious)
				{
					return i;
				}
			}

			return -1;
	}

	float Simulation2D::raymarch(vec2 pos, vec2 direction, const float FAR)
	{
		float d;
		float traveled = 0.0;

		for (int i = 0; i < 100; i++)
		{
			vec2 p = pos + (traveled * direction);

			d = map(p);

			if (d <= -EPSILON)
				break;

			traveled += std::abs(d) + EPSILON;

			if (traveled >= FAR)
			{
				return FAR;
			}
		}

		return traveled - EPSILON;
	}

	void Simulation2D::runSimulationThread()
	{
		while (m_simulating)
		{
			if (m_simulationDelay >= FIXED_DELTA_TIME)
			{
				process();
			}else {
				int delay = std::ceil((FIXED_DELTA_TIME - m_simulationDelay) * 1000); // ms
				std::this_thread::sleep_for(std::chrono::milliseconds(delay));
			}
		}
	}

}
