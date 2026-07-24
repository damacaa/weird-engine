#include "weird-physics/Simulation2D.h"

#include <algorithm>

#include "glm/gtx/norm.hpp"
#include "weird-engine/Logger.h"

namespace WeirdEngine
{

#define MEASURE_PERFORMANCE false
#define INTEGRATION_METHOD 1

	using namespace std::chrono;

	constexpr size_t MAX_STEPS = 10;
	const float EPSILON = 0.0001f;

	Simulation2D::Simulation2D(size_t size, const PhysicsSettings& settings)
		: m_isPaused(false)
		, m_positions(new vec2[size])
		, m_positionsRead(new vec2[size])
		, m_positionsAux(new vec2[size])
		, m_previousPositions(new vec2[size])
		, m_velocities(new vec2[size])
		, m_velocitiesRead(new vec2[size])
		, m_velocitiesAux(new vec2[size])
		, m_forces(new vec2[size])
		, m_impulsesSinceLastUpdate(false)
		, m_impulses(new vec2[size])
		, m_continuousForcesRead(new vec2[size])
		, m_continuousForcesWrite(new vec2[size])
		, m_mass(new float[size])
		, m_invMass(new float[size])
		, m_maxSize(size)
		, m_size(0)
		, m_allocated(0)
		, m_simulationDelay(0)
		, m_simulationTime(0)
		, m_substeps(1)
		, m_simulationFrequency(settings.simulationFrequency)
		, m_fixedDeltaTime(1.0 / static_cast<double>(settings.simulationFrequency))
		, m_fixedDeltaTimeF(static_cast<float>(m_fixedDeltaTime))
		, m_relaxationSteps(settings.relaxationSteps)
		, m_gravity(settings.gravity)
		, m_push(10.0f * settings.simulationFrequency)
		, m_damping(settings.damping)
		, m_simulating(false)
		, m_collisionDetectionMethod(MethodNaive)
		, m_useSimdOperations(false)
		, m_diameter(1.0f)
		, m_diameterSquared(m_diameter * m_diameter)
		, m_radious(m_diameter / 2.0f)
		, m_collisionMap(size)
		, m_head(8191, -1)
	{
		for (size_t i = 0; i < m_maxSize; i++)
		{
			m_positions[i] = vec2(0.0f, 0.0f);
			m_positionsRead[i] = vec2(0.0f, 0.0f);
			m_previousPositions[i] = vec2(0.0f, 0.0f);

			m_velocities[i] = vec2(0.0f);
			m_velocitiesRead[i] = vec2(0.0f);
			m_velocitiesAux[i] = vec2(0.0f);
			m_forces[i] = vec2(0.0f);
			m_continuousForcesRead[i] = vec2(0.0f);
			m_continuousForcesWrite[i] = vec2(0.0f);

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
		delete[] m_velocitiesRead;
		delete[] m_velocitiesAux;
		delete[] m_forces;
		delete[] m_impulses;
		delete[] m_continuousForcesRead;
		delete[] m_continuousForcesWrite;
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

	void Simulation2D::process()
	{
		int steps = 0;

		while (m_simulationDelay >= m_fixedDeltaTime && steps < MAX_STEPS)
		{
			std::lock_guard<std::mutex> structLock(m_structuralMutex);

			std::vector<PhysicsCommand> commandsToExecute;
			{
				std::lock_guard<std::mutex> cmdLock(m_commandMutex);
				commandsToExecute = std::move(m_pendingCommands);
			}
			commandsToExecute.insert(commandsToExecute.end(), m_internalCommands.begin(), m_internalCommands.end());
			m_internalCommands.clear();

			for (const auto& cmd : commandsToExecute)
			{
				switch (cmd.type)
				{
					case PhysicsCommandType::SetVelocity:
						m_velocities[cmd.id] = cmd.vectorData;
						break;
					case PhysicsCommandType::SetPosition:
						m_positions[cmd.id] = cmd.vectorData;
						m_previousPositions[cmd.id] = cmd.vectorData;
						{
							std::lock_guard<std::mutex> rLock(m_readMutex);
							m_positionsRead[cmd.id] = cmd.vectorData;
						}
						break;
					case PhysicsCommandType::SetMass:
						m_mass[cmd.id] = cmd.floatData;
						if (std::find(m_fixedObjects.begin(), m_fixedObjects.end(), cmd.id) == m_fixedObjects.end())
						{
							m_invMass[cmd.id] = cmd.floatData > 0.0f ? 1.0f / cmd.floatData : 0.0f;
						}
						break;
					case PhysicsCommandType::Fix:
						if (std::find(m_fixedObjects.begin(), m_fixedObjects.end(), cmd.id) == m_fixedObjects.end())
						{
							m_fixedObjects.emplace_back(cmd.id);
							m_invMass[cmd.id] = 0.0f;
							m_velocities[cmd.id] = vec2(0.0f);
							m_forces[cmd.id] = vec2(0.0f);
						}
						break;
					case PhysicsCommandType::UnFix:
					{
						auto it = std::find(m_fixedObjects.begin(), m_fixedObjects.end(), cmd.id);
						if (it != m_fixedObjects.end())
						{
							m_fixedObjects.erase(it);
							m_invMass[cmd.id] = m_mass[cmd.id] > 0.0f ? 1.0f / m_mass[cmd.id] : 0.0f;
						}
						break;
					}
					case PhysicsCommandType::ActivatePending:
					{
						m_size = m_allocated;
						break;
					}
					case PhysicsCommandType::AddImpulse:
					{
						std::lock_guard<std::mutex> lock(m_externalForcesMutex);
						m_impulsesSinceLastUpdate = true;
						m_impulses[cmd.id] += cmd.vectorData;
						break;
					}
					default:
						break;
				}
			}

			auto start = std::chrono::high_resolution_clock::now();

			if (!m_isPaused)
			{
				{
					std::lock_guard<std::mutex> shapeLock(m_shapeUpdateMutex);
					for (auto& cmd : m_pendingShapeUpdates)
					{
						if (cmd.isRemove)
						{
							internalRemoveShape(cmd.owner, cmd.shape);
						}
						else
						{
							internalUpdateShape(cmd.owner, cmd.shape);
						}
					}
					m_pendingShapeUpdates.clear();
				}

				double timerBroad = 0, timerNarrow = 0, timerShape = 0;
				checkCollisions(timerBroad, timerNarrow, timerShape);

				auto timerForceStart = std::chrono::high_resolution_clock::now();
				applyForces();
				auto timerForceEnd = std::chrono::high_resolution_clock::now();
				double timerCollisionEvents =
					std::chrono::duration<double, std::milli>(timerForceEnd - timerForceStart).count();

				auto timerIntStart = std::chrono::high_resolution_clock::now();
				// 1. Predict where particles will go based on velocity and forces
				integratePredict((float)m_fixedDeltaTime);

				// 2. Iteratively solve constraints (Push/Pull particles to their exact distances)

				for (int iter = 0; iter < m_relaxationSteps; iter++)
				{
					solveConstraints();
				}

				// 3. Derive the exact velocity based on how much the constraints moved the particles
				integrateVelocity((float)m_fixedDeltaTime);
				auto timerIntEnd = std::chrono::high_resolution_clock::now();
				double timerIntegration =
					std::chrono::duration<double, std::milli>(timerIntEnd - timerIntStart).count();

				{
					std::lock_guard<std::mutex> lock(m_statsMutex);
					// Exponential moving average (10% new value, 90% old value)
					m_stats.broadPhaseMs = m_stats.broadPhaseMs * 0.9 + timerBroad * 0.1;
					m_stats.narrowPhaseMs = m_stats.narrowPhaseMs * 0.9 + timerNarrow * 0.1;
					m_stats.shapeEvaluationMs = m_stats.shapeEvaluationMs * 0.9 + timerShape * 0.1;
					m_stats.collisionEventsMs = m_stats.collisionEventsMs * 0.9 + timerCollisionEvents * 0.1;
					m_stats.integrationMs = m_stats.integrationMs * 0.9 + timerIntegration * 0.1;
				}

				++steps;
				{
					// m_simulationTime += m_fixedDeltaTime;
					// m_simulationTime.fetch_add(m_fixedDeltaTime);
					double current = m_simulationTime.load();
					while (!m_simulationTime.compare_exchange_weak(current, current + m_fixedDeltaTime))
					{
						// If another thread modifies m_simulationTime before we do,
						// compare_exchange_weak fails, updates 'current' to the new value,
						// and the loop tries again.
					}

					// Notify collision callback
					if (m_stepCallback)
					{
						m_stepCallback(m_callbackUserData);
					}
				}
			}

			{
				// std::lock_guard<std::mutex> lock(g_simulationTimeMutex); // Lock the mutex
				m_simulationDelay -= m_fixedDeltaTime;
			}

			auto end = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> durationMs = end - start;

			{
				std::lock_guard<std::mutex> lock(m_statsMutex);
				m_stats.timePerStepMs = durationMs.count();
				m_stats.simulationRatio =
					durationMs.count() > 0.0 ? (m_fixedDeltaTime * 1000.0) / durationMs.count() : 0.0;
			}
		}
	}

	double Simulation2D::getSimulationTime()
	{
		// std::lock_guard<std::mutex> lock(g_simulationTimeMutex);
		return m_simulationTime;
	}

	void Simulation2D::startSimulationThread()
	{
		process(); // Process any pending before starting the thread

		m_simulating = true;

		m_simulationThread = std::thread(&Simulation2D::runSimulationThread, this);
		m_physicsThreadId = m_simulationThread.get_id();
	}

	void Simulation2D::stopSimulationThread()
	{
		if (!m_simulating)
			return;

		m_simulating = false;
		m_simulationThread.join();
	}

	void Simulation2D::checkCollisions(double& broadPhaseMs, double& narrowPhaseMs, double& shapeEvaluationMs)
	{
		auto timerStart = std::chrono::high_resolution_clock::now();
		// Detect collisions

		m_collisions.clear();

		// Spatial hash grid (broadphase)
		const int TABLE_SIZE = 8191; // A prime number for better hashing

		// Reset the head array for this frame
		std::fill(m_head.begin(), m_head.end(), -1);

		// Ensure the next array is large enough to hold all particles
		if (m_next.size() < m_size)
		{
			m_next.resize(std::max((size_t)m_size, m_maxSize), -1);
		}

		// Cell size should be exactly the maximum interaction distance (the diameter)
		float invCellSize = 1.0f / m_diameter;

		// Fast hash function to convert a 2D grid coordinate into an array index
		auto getHash = [](int gx, int gy) -> int
		{
			constexpr int p1 = 73856093;
			constexpr int p2 = 19349663;
			int hash = (gx * p1) ^ (gy * p2);
			hash = hash % TABLE_SIZE;
			if (hash < 0)
				hash += TABLE_SIZE; // Handle negative coordinates safely
			return hash;
		};

		// Insert all particles into the spatial grid
		for (int i = 0; i < m_size; i++)
		{
			// Calculate which grid cell the particle is in
			int gx = static_cast<int>(std::floor(m_positions[i].x * invCellSize));
			int gy = static_cast<int>(std::floor(m_positions[i].y * invCellSize));

			int hash = getHash(gx, gy);

			// Insert at the head of the linked list for this cell
			m_next[i] = m_head[hash];
			m_head[hash] = i;
		}

		// Update the spatial grid snapshot for read-only access (e.g. by raymarching in the main thread)
		{
			// TODO: [PERFORMANCE] Avoid heap allocation every physics step.
			// Currently, we allocate a new std::shared_ptr and its internal std::vector buffers
			// (head, next, positions) on the heap every single time this function runs.
			// While std::shared_ptr safely manages the memory lifecycle for the main thread,
			// this causes unnecessary memory fragmentation and malloc overhead.
			//
			// PROPOSED SOLUTION: Implement an Object Pool (e.g. std::vector<SpatialGridSnapshot*>)
			// with a custom std::shared_ptr deleter. Instead of allocating a new object here,
			// pop an old one from the pool (which reuses the vector capacities). When the main
			// thread's shared_ptr reference count drops to 0, the custom deleter should push
			// the object back into the pool instead of deleting it.
			auto snapshot = std::make_shared<SpatialGridSnapshot>();
			snapshot->head = m_head;
			snapshot->next = m_next;
			snapshot->positions.assign(m_positions, m_positions + m_size);
			snapshot->invCellSize = invCellSize;
			snapshot->radious = m_radious;

			std::lock_guard<std::mutex> lock(m_spatialGridSnapshotMutex);
			m_spatialGridSnapshot = snapshot;
		}

		auto timerBroad = std::chrono::high_resolution_clock::now();
		broadPhaseMs = std::chrono::duration<double, std::milli>(timerBroad - timerStart).count();

		// Check for collisions using the grid
		for (int i = 0; i < m_size; i++)
		{
			int gx = static_cast<int>(std::floor(m_positions[i].x * invCellSize));
			int gy = static_cast<int>(std::floor(m_positions[i].y * invCellSize));

			// Only check the 9 neighboring cells (including the particle's own cell)
			for (int dx = -1; dx <= 1; dx++)
			{
				for (int dy = -1; dy <= 1; dy++)
				{
					int hash = getHash(gx + dx, gy + dy);
					int j = m_head[hash];

					// Traverse the linked list of particles in this cell
					while (j != -1)
					{
						// Prevents checking a particle against itself
						// Ensures we only check each pair once
						if (i < j)
						{
							vec2 ij = m_positions[j] - m_positions[i];
							float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

							if (distanceSquared < m_diameterSquared)
							{
								m_collisions.emplace_back(Collision(i, j, ij));
							}
						}

						j = m_next[j]; // Move to the next particle in the cell
					}
				}
			}
		}

		auto timerNarrow = std::chrono::high_resolution_clock::now();
		narrowPhaseMs = std::chrono::duration<double, std::milli>(timerNarrow - timerBroad).count();

		// Shape collisions
		for (size_t i = 0; i < m_size; i++)
		{
			vec2& p = m_positions[i];

			// Check
			bool currentCollision = false;
			ShapeCollisionEvent collisionEvent;
			collisionEvent.body = static_cast<SimulationID>(i);

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

				float distanceAtSurface = map(p - ((m_radious)*collisionEvent.normal));
				if (distanceAtSurface <=
					0.0f) // Bad solution? Check if the distance at approximate contact point is small enough
				{
					float penetration = (std::min)(-distanceAtSurface, m_radious - d);
					currentCollision = true;

					collisionEvent.penetration = penetration;
					collisionEvent.position = p - (0.5f * collisionEvent.normal);
					collisionEvent.velocity = m_velocities[i];

					// TODO: Pre-calculate target plane for relaxation
					// collisionEvent.targetPos = p + (penetration * collisionEvent.normal);

					constexpr float DYNAMIC_FRICTION = 0.05f;
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

		auto timerShape = std::chrono::high_resolution_clock::now();
		shapeEvaluationMs = std::chrono::duration<double, std::milli>(timerShape - timerNarrow).count();
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

	// static float fOpUnionSoft(float a, float b, float r)
	// {
	// 	r *= 1.0f; // 4.0f orignal wtf
	// 	float h = std::max(r - abs(a - b), 0.0f);
	// 	return std::min(a, b) - h * h * 0.25f / r;
	// }

	// // Smooth subtraction (a - b), 2D SDF
	// static float fOpSubSoft(float a, float b, float r)
	// {
	// 	return -fOpUnionSoft(b, -a, r);
	// }

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

		struct GroupState
		{
			uint16_t id;
			float minDistance;
		};

		// We typically have very few groups; linear lookup avoids hash overhead.
		std::vector<GroupState> groups;
		groups.reserve(16);

		std::vector<int> globalShapes;

		for (int i = 0; i < m_objects.size(); i++)
		{
			DistanceFieldObject2D& obj = m_objects[i];
			if (obj.groupId == CustomShape::GLOBAL_GROUP)
			{
				globalShapes.push_back(i);
				continue;
			}

			if (obj.distanceFieldId >= m_sdfs->size())
			{
				continue;
			}

			obj.parameters[8] = static_cast<float>(m_simulationTime);
			obj.parameters[9] = p.x;
			obj.parameters[10] = p.y;

			// Distance

			float dist = (*m_sdfs)[obj.distanceFieldId]->getValue(obj.parameters);

			float currentMinDistance = d;
			GroupState* groupState = nullptr;

			for (auto& group : groups)
			{
				if (group.id == obj.groupId)
				{
					groupState = &group;
					break;
				}
			}

			if (groupState == nullptr)
			{
				groups.push_back({obj.groupId, 100000.0f});
				groupState = &groups.back();
			}

			currentMinDistance = groupState->minDistance;

			// Combination
			switch (obj.combinationId)
			{
				case CombinationType::Addition:
				{
					currentMinDistance = std::min(currentMinDistance, dist);
					break;
				}
				case CombinationType::Subtraction:
				{
					currentMinDistance = std::max(currentMinDistance, -dist);
					break;
				}
				case CombinationType::Intersection:
				{
					currentMinDistance = std::max(currentMinDistance, dist);
					break;
				}
				case CombinationType::SmoothAddition:
				{
					currentMinDistance = fOpUnionSoft(currentMinDistance, dist, 1.0f);
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
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

			groupState->minDistance = currentMinDistance;
		}

		for (const auto& group : groups)
		{
			d = std::min(d, group.minDistance);
		}

		// Apply global shapes as well, but without grouping (they affect everything)
		for (int shapeIdx : globalShapes)
		{
			DistanceFieldObject2D& obj = m_objects[shapeIdx];

			if (obj.distanceFieldId >= m_sdfs->size())
			{
				continue;
			}

			obj.parameters[8] = static_cast<float>(m_simulationTime);
			obj.parameters[9] = p.x;
			obj.parameters[10] = p.y;

			// Distance

			float dist = (*m_sdfs)[obj.distanceFieldId]->getValue(obj.parameters);

			float currentMinDistance = d;

			// Combination
			switch (obj.combinationId)
			{
				case CombinationType::Addition:
				{
					currentMinDistance = std::min(currentMinDistance, dist);
					break;
				}
				case CombinationType::Subtraction:
				{
					currentMinDistance = std::max(currentMinDistance, -dist);
					break;
				}
				case CombinationType::Intersection:
				{
					currentMinDistance = std::max(currentMinDistance, dist);
					break;
				}
				case CombinationType::SmoothAddition:
				{
					currentMinDistance = fOpUnionSoft(currentMinDistance, dist, 1.0f);
					break;
				}
				case CombinationType::SmoothSubtraction:
				{
					currentMinDistance = fOpSubSoft(currentMinDistance, dist, 1.0f);
					break;
				}
				default:
					break;
			}

			if (currentMinDistance < minD)
			{
				minD = currentMinDistance;
				closestShape = shapeIdx;
			}

			d = currentMinDistance;
		}

		return d - 0.05f;
	}

	void Simulation2D::applyForces()
	{
		// External forces
		{
			std::lock_guard<std::mutex> lock(m_externalForcesMutex);

			for (size_t i = 0; i < m_size; i++)
			{
				m_forces[i] += m_continuousForcesRead[i];
			}

			if (m_impulsesSinceLastUpdate)
			{
				m_impulsesSinceLastUpdate = false;
				for (size_t i = 0; i < m_size; i++)
				{
					m_forces[i] += m_impulses[i];
					m_impulses[i] = vec2(0);
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

			// Only apply impulse if objects are moving towards each other
			if (velocityAlongNormal < 0.0f)
			{
				float invMassSum = m_invMass[col.A] + m_invMass[col.B];
				if (invMassSum > 0.0f)
				{
					float impulseMagnitude = -(1 + restitution) * velocityAlongNormal / invMassSum;
					vec2 impulse = impulseMagnitude * normal;

					m_velocities[col.A] -= m_invMass[col.A] * impulse;
					m_velocities[col.B] += m_invMass[col.B] * impulse;
				}
			}

			// Penalty method
			vec2 penalty = m_push * penetration * normal;
			m_forces[col.A] -= m_mass[col.A] * penalty;
			m_forces[col.B] += m_mass[col.B] * penalty;

			// Notify collision callback
			if (m_collisionCallback)
			{
				CollisionEvent event{col.A, col.B};
				m_collisionCallback(event, m_callbackUserData);
			}
		}

		// Shape collisions
		for (auto& collisionEvent : m_collisionQueue)
		{
			// Send event
			if (m_shapeCollisionCallback)
			{
				m_shapeCollisionCallback(collisionEvent, m_callbackUserData); // Scene can modify values
			}

			if (collisionEvent.state == CollisionState::END)
				continue;

			// Use the current velocity of the body for accurate response
			vec2 vel = m_velocities[collisionEvent.body];

			float v_n = glm::dot(vel, collisionEvent.normal);
			vec2 vel_t = vel - (v_n * collisionEvent.normal);
			float speed_t = length(vel_t);

			// Apply Tangential Friction
			if (speed_t > EPSILON)
			{
				// Normal acceleration from the penalty method (calculated below: push * penetration^2)
				float normalAcceleration = m_push * collisionEvent.penetration * collisionEvent.penetration;

				// Coulomb friction (constant sliding resistance based on normal force)
				float coulombDrop = collisionEvent.friction * normalAcceleration * m_fixedDeltaTimeF;

				// Viscous friction (increases with speed to slow it down more when moving fast)
				float viscousDrop = collisionEvent.friction * speed_t * 10.0f * m_fixedDeltaTimeF;

				float totalDrop = coulombDrop + viscousDrop;

				// Clamp velocity drop so it never reverses the direction (fixes low-speed jitter)
				float drop = std::min(totalDrop, speed_t);

				m_velocities[collisionEvent.body] -= drop * (vel_t / speed_t);
			}

			// Absorption (Damping on the normal axis to prevent infinite bouncing)
			// Apply damping proportional to the normal velocity
			float dampingDrop = collisionEvent.absortion * v_n * m_fixedDeltaTimeF;
			if (v_n > 0.0f)
			{
				dampingDrop = std::min(dampingDrop, v_n);
			}
			else
			{
				dampingDrop = std::max(dampingDrop, v_n);
			}
			m_velocities[collisionEvent.body] -= dampingDrop * collisionEvent.normal;

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
		for (auto it = m_distanceConstraints.begin(); it != m_distanceConstraints.end(); ++it)
		{
			DistanceConstraint constraint = *it;

			vec2 v = m_positions[constraint.B] - m_positions[constraint.A];
			float distance = length(v);
			if (distance <= EPSILON)
				continue; // Prevent division by zero

			vec2 n = v / distance; // Normalized direction

			// Get inverse masses
			float wA = m_invMass[constraint.A];
			float wB = m_invMass[constraint.B];
			float wSum = wA + wB;

			if (wSum == 0.0f)
				continue; // Both objects are infinite mass/fixed

			// How far off are we?
			float error = distance - constraint.Distance;

			// Calculate correction proportional to mass
			vec2 correctionVector = n * (error / wSum);

			float stiffness = constraint.K; // 1.0f -> 100% correction per iteration

			m_positions[constraint.A] += wA * correctionVector * stiffness;
			m_positions[constraint.B] -= wB * correctionVector * stiffness;
		}

		// TODO: implement this position based or move out of relaxation
		// for (auto it = m_gravitationalConstraints.begin(); it != m_gravitationalConstraints.end(); ++it)
		// {
		// 	GravitationalConstraint constraint = *it;
		//
		// 	vec2 v = m_positions[constraint.B] - m_positions[constraint.A];
		// 	vec2 n = normalize(v);
		// 	float distance = length(v);
		//
		// 	float f = constraint.g * (m_mass[constraint.A] * m_mass[constraint.B]) / (distance * distance);
		//
		// 	m_forces[constraint.A] += f * n;
		// 	m_forces[constraint.B] -= f * n;
		// }
	}

	void Simulation2D::integratePredict(const float timeStep)
	{
		for (size_t i = 0; i < m_size; i++)
		{
			// Store current position
			m_previousPositions[i] = m_positions[i];

			// Apply forces to velocity (v = v + a*dt)
			vec2 acc = m_forces[i] * m_invMass[i];
			m_velocities[i] += acc * timeStep;

			// Predict new position (p = p + v*dt)
			m_positions[i] += m_velocities[i] * timeStep;

			// Clear forces for next frame
			m_forces[i] = vec2(0.0f);
		}
	}

	void Simulation2D::integrateVelocity(const float timeStep)
	{
		float invTimeStep = 1.0f / timeStep;
		for (size_t i = 0; i < m_size; i++)
		{
			// How much did the particle actually move after constraints pushed it around?
			vec2 newVelocity = (m_positions[i] - m_previousPositions[i]) * invTimeStep;

			// Apply damping and store
			m_velocities[i] = newVelocity * (1.0f - m_damping);
		}

		// Restore your original rendering buffer logic
		for (size_t i = 0; i < m_size; i++)
		{
			m_positionsAux[i] = m_positions[i];
			m_velocitiesAux[i] = m_velocities[i];
		}

		// swap buffers
		{
			std::lock_guard<std::mutex> lock(m_readMutex);
			vec2* aux = m_positionsAux;
			m_positionsAux = m_positionsRead;
			m_positionsRead = aux;

			aux = m_velocitiesAux;
			m_velocitiesAux = m_velocitiesRead;
			m_velocitiesRead = aux;
		}
	}

#pragma endregion

	SimulationID Simulation2D::generateSimulationID()
	{
		std::lock_guard<std::mutex> lock(m_structuralMutex);

		SimulationID id = static_cast<SimulationID>(m_allocated);

		// Initialize particle with safe defaults so the physics
		// thread never processes stale/garbage data.
		m_positions[id] = vec2(0.0f);
		m_positionsRead[id] = vec2(0.0f);
		m_positionsAux[id] = vec2(0.0f);
		m_previousPositions[id] = vec2(0.0f);
		m_velocities[id] = vec2(0.0f);
		m_velocitiesRead[id] = vec2(0.0f);
		m_velocitiesAux[id] = vec2(0.0f);
		m_forces[id] = vec2(0.0f);
		m_impulses[id] = vec2(0.0f);
		m_continuousForcesRead[id] = vec2(0.0f);
		m_continuousForcesWrite[id] = vec2(0.0f);
		m_mass[id] = 1.0f;
		m_invMass[id] = 1.0f;
		m_collisionMap[id] = false;

		m_allocated++;
		return id;
	}

	void Simulation2D::activatePendingBodies()
	{
		if (m_allocated == m_size)
			return;

		std::lock_guard<std::mutex> lock(m_commandMutex);
		m_pendingCommands.push_back({PhysicsCommandType::ActivatePending});
	}

	void Simulation2D::removeObject(SimulationID id)
	{
		std::scoped_lock lock(m_structuralMutex, m_externalForcesMutex, m_fixMutex);

		if (m_size == 0 || id >= m_size)
		{
			return;
		}

		auto toId = id;
		auto fromId = m_size - 1;

		if (toId != fromId)
		{
			m_positions[toId] = m_positions[fromId];
			m_positionsRead[toId] = m_positionsRead[fromId];
			m_positionsAux[toId] = m_positionsAux[fromId];
			m_previousPositions[toId] = m_previousPositions[fromId];
			m_velocities[toId] = m_velocities[fromId];
			m_velocitiesRead[toId] = m_velocitiesRead[fromId];
			m_velocitiesAux[toId] = m_velocitiesAux[fromId];
			m_forces[toId] = m_forces[fromId];
			m_impulses[toId] = m_impulses[fromId];
			m_continuousForcesRead[toId] = m_continuousForcesRead[fromId];
			m_continuousForcesWrite[toId] = m_continuousForcesWrite[fromId];
			m_mass[toId] = m_mass[fromId];
			m_invMass[toId] = m_invMass[fromId];

			if (toId < m_collisionMap.size() && fromId < m_collisionMap.size())
			{
				m_collisionMap[toId] = m_collisionMap[fromId];
			}
		}

		// Fix constraints (potentially slow...)

		// Remove constraints that affect deleted object
		auto RemoveByID = [toId](auto& container)
		{
			container.erase(std::remove_if(container.begin(), container.end(), [toId](const auto& constraint)
										   { return constraint.A == toId || constraint.B == toId; }),
							container.end());
		};

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
					constraint.A = toId;
				}
				if (constraint.B == fromId)
				{
					constraint.B = toId;
				}
			}
		};

		ChangeByID(m_distanceConstraints);
		ChangeByID(m_gravitationalConstraints);

		// Find if moved object is fixed
		auto it = std::find(m_fixedObjects.begin(), m_fixedObjects.end(), fromId);

		// If it is, fix it at its new id
		if (it != m_fixedObjects.end())
		{
			int index = static_cast<int>(it - m_fixedObjects.begin());
			m_fixedObjects[index] = toId;
		}

		// Adjust size
		m_size--;
		m_allocated--;
	}

	size_t Simulation2D::getSize()
	{
		return m_size;
	}

	void Simulation2D::addImpulseForce(SimulationID id, const vec2& impulse, bool massIndependent)
	{
		vec2 finalImpulse = impulse * m_simulationFrequency;
		if (massIndependent)
			finalImpulse *= m_mass[id];

		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back({PhysicsCommandType::AddImpulse, id, finalImpulse});
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back({PhysicsCommandType::AddImpulse, id, finalImpulse});
		}
	}

	void Simulation2D::setContinuousForce(SimulationID id, const vec2& force, bool massIndependent)
	{
		// No lock needed, game thread writes directly to the write buffer
		if (massIndependent)
			m_continuousForcesWrite[id] = force * m_mass[id];
		else
			m_continuousForcesWrite[id] = force;
	}

	void Simulation2D::swapContinuousForces()
	{
		std::lock_guard<std::mutex> lock(m_externalForcesMutex);

		vec2* temp = m_continuousForcesRead;
		m_continuousForcesRead = m_continuousForcesWrite;
		m_continuousForcesWrite = temp;

		// Clear the new write buffer so it's ready for the next frame
		for (size_t i = 0; i < m_allocated; i++)
		{
			m_continuousForcesWrite[i] = vec2(0.0f);
		}
	}

	void Simulation2D::addSpring(SimulationID a, SimulationID b, float stiffness, float distance)
	{
		if (a == b)
			return;

		std::lock_guard<std::mutex> lock(m_structuralMutex);
		m_distanceConstraints.emplace_back(
			a, b, distance,
			static_cast<float>(
				std::pow(stiffness, std::sqrt(m_relaxationSteps)))); // Square to make stiffness more intuitive
	}

	void Simulation2D::addPositionConstraint(SimulationID a, SimulationID b, float distance)
	{
		if (a == b)
			return;

		std::lock_guard<std::mutex> lock(m_structuralMutex);
		m_distanceConstraints.emplace_back(a, b, distance, 1.0f);
	}

	void Simulation2D::addGravitationalConstraint(SimulationID a, SimulationID b, float gravity)
	{
		if (a == b)
			return;

		std::lock_guard<std::mutex> lock(m_structuralMutex);
		m_gravitationalConstraints.emplace_back(a, b, gravity);
	}

	bool Simulation2D::setDistanceConstraintDistance(SimulationID a, SimulationID b, float distance)
	{
		if (a == b)
			return false;

		std::lock_guard<std::mutex> lock(m_structuralMutex);
		for (auto& constraint : m_distanceConstraints)
		{
			bool sameDirection = (constraint.A == static_cast<int>(a) && constraint.B == static_cast<int>(b));
			bool reverseDirection = (constraint.A == static_cast<int>(b) && constraint.B == static_cast<int>(a));
			if (sameDirection || reverseDirection)
			{
				constraint.Distance = distance;
				return true;
			}
		}

		return false;
	}

	bool Simulation2D::removeDistanceConstraint(SimulationID a, SimulationID b)
	{
		if (a == b)
			return false;

		std::lock_guard<std::mutex> lock(m_structuralMutex);

		size_t previousSize = m_distanceConstraints.size();
		m_distanceConstraints.erase(std::remove_if(m_distanceConstraints.begin(), m_distanceConstraints.end(),
												   [a, b](const DistanceConstraint& constraint)
												   {
													   bool sameDirection = (constraint.A == static_cast<int>(a) &&
																			 constraint.B == static_cast<int>(b));
													   bool reverseDirection = (constraint.A == static_cast<int>(b) &&
																				constraint.B == static_cast<int>(a));
													   return sameDirection || reverseDirection;
												   }),
									m_distanceConstraints.end());

		return previousSize != m_distanceConstraints.size();
	}

	void Simulation2D::fix(SimulationID id)
	{
		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back({PhysicsCommandType::Fix, id});
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back({PhysicsCommandType::Fix, id});
		}
	}

	void Simulation2D::unFix(SimulationID id)
	{
		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back({PhysicsCommandType::UnFix, id});
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back({PhysicsCommandType::UnFix, id});
		}
	}

	bool Simulation2D::isFixed(SimulationID id)
	{
		std::lock_guard<std::mutex> lock(m_structuralMutex);
		return std::find(m_fixedObjects.begin(), m_fixedObjects.end(), id) != m_fixedObjects.end();
	}

	vec2 Simulation2D::getPosition(SimulationID entity)
	{
		std::lock_guard<std::mutex> lock(m_readMutex);
		return m_positionsRead[entity];
	}

	void Simulation2D::setPosition(SimulationID id, vec2 pos)
	{
		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back({PhysicsCommandType::SetPosition, id, pos});
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back({PhysicsCommandType::SetPosition, id, pos});

			std::lock_guard<std::mutex> readLock(m_readMutex);
			m_positionsRead[id] = pos;
		}
	}

	vec2 Simulation2D::getVelocity(SimulationID id)
	{
		return m_velocitiesRead[id];
	}

	void Simulation2D::setVelocity(SimulationID id, vec2 vel)
	{
		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back({PhysicsCommandType::SetVelocity, id, vel});
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back({PhysicsCommandType::SetVelocity, id, vel});

			std::lock_guard<std::mutex> readLock(m_readMutex);
			m_velocitiesRead[id] = vel;
		}
	}

	void Simulation2D::updateTransform(Transform& transform, SimulationID id)
	{
		std::lock_guard<std::mutex> lock(m_readMutex);
		transform.position.x = m_positionsRead[id].x;
		transform.position.y = m_positionsRead[id].y;
	}

	void Simulation2D::setMass(SimulationID id, float mass)
	{
		PhysicsCommand cmd = {PhysicsCommandType::SetMass, id};
		cmd.floatData = mass;

		if (std::this_thread::get_id() == m_physicsThreadId)
		{
			m_internalCommands.push_back(cmd);
		}
		else
		{
			std::lock_guard<std::mutex> lock(m_commandMutex);
			m_pendingCommands.push_back(cmd);
		}
	}

	void Simulation2D::setSDFs(std::vector<std::shared_ptr<IMathExpression>>& sdfs)
	{
		m_sdfs = std::make_shared<std::vector<std::shared_ptr<IMathExpression>>>(sdfs);
	}

	void Simulation2D::internalUpdateShape(Entity owner, CustomShape& shape)
	{
		if (!shape.hasCollisions)
			return;

		DistanceFieldObject2D sdf(owner, shape.distanceFieldId, shape.combination, shape.groupIdx, shape.parameters);

		// Check if the key exists
		auto it = m_entityToObjectsIdx.find(owner);
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
			ShapeId id = static_cast<ShapeId>(m_objects.size() - 1);
			m_entityToObjectsIdx[owner] = id;
			shape.simulationId = id;
		}
	}

	void Simulation2D::internalRemoveShape(Entity owner, CustomShape& shape)
	{
		if (m_objects.size() == 0)
		{
			return;
		}

		auto it = m_entityToObjectsIdx.find(owner);
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
		m_entityToObjectsIdx.erase(owner);
	}

	void Simulation2D::updateShape(Entity owner, CustomShape& shape)
	{
		std::lock_guard<std::mutex> lock(m_shapeUpdateMutex);
		m_pendingShapeUpdates.push_back({false, owner, shape});
	}

	void Simulation2D::removeShape(Entity owner, CustomShape& shape)
	{
		std::lock_guard<std::mutex> lock(m_shapeUpdateMutex);
		m_pendingShapeUpdates.push_back({true, owner, shape});
	}

	SimulationID Simulation2D::raycast(vec2 pos)
	{
		for (size_t i = 0; i < m_size; i++)
		{
			vec2 ij = pos - m_positions[i];

			float distanceSquared = (ij.x * ij.x) + (ij.y * ij.y);

			if (distanceSquared < m_radious * m_radious)
			{
				return static_cast<SimulationID>(i);
			}
		}

		return -1;
	}

	float Simulation2D::raymarch(vec2 pos, vec2 direction, const float FAR)
	{
		int closestShape;

		return raymarch(pos, direction, FAR, closestShape);
	}

	float Simulation2D::raymarch(vec2 pos, vec2 direction, const float FAR, int& closestShape)
	{
		float d;
		float traveled = 0.0;

		for (int i = 0; i < 100; i++)
		{
			vec2 p = pos + (traveled * direction);

			d = map(p, closestShape);

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
			if (m_simulationDelay >= m_fixedDeltaTime)
			{
				process();
			}
			else
			{
				int delay = static_cast<int>(std::ceil((m_fixedDeltaTime - m_simulationDelay) * 1000)); // ms
				std::this_thread::sleep_for(std::chrono::milliseconds(delay));
			}
		}
	}

} // namespace WeirdEngine
