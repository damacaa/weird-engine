#pragma once
#include "weird-engine/ecs/ECS.h"
#include "weird-engine/Input.h"


namespace WeirdEngine
{
	using namespace ECS;

	namespace PhysicsInteractionSystem
	{

		inline bool m_loadingImpulse = false;
		inline vec2 m_loadStartPosition;

		inline Entity m_dragId = INVALID_ENTITY;

		inline int m_currentMaterial = 0;

		inline SimulationID m_firstIdInSpring = -1;

		inline SimulationID m_selectedId = -1;

		enum class InteractionMode
		{
			Drag = 0,
			Impulse = 1,
			Fix = 2,
			Spring = 3,
			DistanceConstraint = 4,
		};

		inline std::string m_interactionModeToString[5] = {"Drag", "Impulse", "Fix", "Spring", "DistanceConstraint"};

		inline InteractionMode m_currentInteractionMode = InteractionMode::Drag;

		inline vec2 getMousePositionInWorld(ECSManager& ecs);
		inline void drag(ECSManager& ecs);
		inline void impulse(ECSManager& ecs);
		inline void fix(ECSManager& ecs);
		inline void spring(ECSManager& ecs);
		inline void positionConstraint(ECSManager& ecs);

		inline void reset()
		{
			WeirdEngine::Logger::log(m_interactionModeToString[(int)m_currentInteractionMode]);

			m_loadingImpulse = false;

			m_dragId = INVALID_ENTITY;

			m_currentMaterial = 0;

			m_firstIdInSpring = INVALID_ENTITY;

			m_selectedId = INVALID_ENTITY;
		}

		inline void update(ECSManager& ecs)
		{
			// Spawn ball
			if (Input::GetMouseButtonDown(Input::LeftClick))
			{
				// Get mouse coordinates world space
				vec2 mousePositionInWorld = getMousePositionInWorld(ecs);

				// t.position = vec3(mousePositionInWorld.x + sin(time), mousePositionInWorld.y + cos(time), 0.0);
				Entity entity = ecs.createEntity();
				Transform& t = ecs.addComponent<Transform>(entity);
				t.position = vec3(mousePositionInWorld.x, mousePositionInWorld.y, 0.0);

				auto& dot = ecs.addComponent<Dot>(entity);
				dot.materialId = m_currentMaterial + 4;

				RigidBody2D& rb = ecs.addComponent<RigidBody2D>(entity);
				rb.pendingImpulseForce += 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY());

				m_firstIdInSpring = INVALID_ENTITY;
			}

			if (Input::GetKeyDown(Input::C))
			{
				m_currentMaterial = (m_currentMaterial + 1) % 12;
			}

			switch (m_currentInteractionMode)
			{
				case PhysicsInteractionSystem::InteractionMode::Drag:
					drag(ecs);
					break;
				case PhysicsInteractionSystem::InteractionMode::Impulse:
					impulse(ecs);
					break;
				case PhysicsInteractionSystem::InteractionMode::Fix:
					fix(ecs);
					break;
				case PhysicsInteractionSystem::InteractionMode::Spring:
					spring(ecs);
					break;
				case PhysicsInteractionSystem::InteractionMode::DistanceConstraint:
					positionConstraint(ecs);
					break;
				default:
					break;
			}

			if (Input::GetKeyDown(Input::Num1))
			{
				m_currentInteractionMode = InteractionMode::Drag;
				reset();
			}

			if (Input::GetKeyDown(Input::Num2))
			{
				m_currentInteractionMode = InteractionMode::Impulse;
				reset();
			}

			if (Input::GetKeyDown(Input::Num3))
			{
				m_currentInteractionMode = InteractionMode::Fix;
				reset();
			}

			if (Input::GetKeyDown(Input::Num4))
			{
				m_currentInteractionMode = InteractionMode::Spring;
				reset();
			}

			if (Input::GetKeyDown(Input::Num5))
			{
				m_currentInteractionMode = InteractionMode::DistanceConstraint;
				reset();
			}

			// Add force to last ball
			if (Input::GetKey(Input::T))
			{
				auto rbs = ecs.getComponentArray<RigidBody2D>();

				RigidBody2D& target = rbs->getLastData();
				Entity targetOwner = rbs->getEntityAtIdx(rbs->getSize() - 1);

				vec3 position = ecs.getComponent<Transform>(targetOwner).position;
				vec2 v = vec2(15, 30) - vec2(position.x, position.y);

				target.pendingContinuousForce += 50.0f * normalize(v);

				rbs->setEntityDirty(targetOwner, true);
			}

			// // Pause / resume simulation
			// if (Input::GetKeyDown(Input::P))
			// {
			// 	if (simulation.isPaused())
			// 	{
			// 		simulation.resume();
			// 	}
			// 	else
			// 	{
			// 		simulation.pause();
			// 	}
			// }
		}

		inline vec2 getMousePositionInWorld(ECSManager& ecs)
		{
			// Get mouse coordinates
			auto& cameraTransform = ecs.getComponent<Transform>(0); // m_mainCamera
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			return mousePositionInWorld;
		}

		inline void drag(ECSManager& ecs)
		{
			if (Input::GetMouseButtonDown(Input::RightClick))
			{
				auto rbs = ecs.getComponentArray<RigidBody2D>();


				float minD2 = 1.0f;

				vec2 mouseInWorld = getMousePositionInWorld(ecs);

				for (int i = 0; i < rbs->getSize(); i++)
				{
					auto& rb = rbs->getDataAtIdx(i);
					Entity rbOwner = rbs->getEntityAtIdx(i);
					auto& t = ecs.getComponent<Transform>(rbOwner);

					float d2 = glm::length2(mouseInWorld - vec2(t.position.x, t.position.y));
					if (d2 < minD2)
					{
						minD2 = d2;
						m_dragId = rbOwner;
					}
				}


				if(m_dragId != INVALID_ENTITY)
				{
					auto& rb = ecs.getComponent<RigidBody2D>(m_dragId);
					rb.isFixed = true;
					rbs->setEntityDirty(m_dragId, true);
				}
				
			}

			if (Input::GetMouseButtonUp(Input::RightClick))
			{
				if (m_dragId != INVALID_ENTITY)
				{
					auto& rb = ecs.getComponent<RigidBody2D>(m_dragId);
					rb.isFixed = false;
					rb.pendingImpulseForce += 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY());
					ecs.getComponentArray<RigidBody2D>()->setEntityDirty(m_dragId, true);

					m_dragId = INVALID_ENTITY;
				}
			}

			if (m_dragId != INVALID_ENTITY)
			{
				vec2 mousePositionInWorld = getMousePositionInWorld(ecs);
				auto& t = ecs.getComponent<Transform>(m_dragId);

				t.position = vec3(mousePositionInWorld, 0.0f);
				ecs.getComponentArray<Transform>()->setEntityDirty(m_dragId, true);
			}
		}

		inline Entity getEntityAtPosition(ECSManager& ecs, vec2 position, float radius = 0.5f)
		{
			auto rbs = ecs.getComponentArray<RigidBody2D>();
			float minD2 = radius * radius;
			Entity found = INVALID_ENTITY;

			for (int i = 0; i < rbs->getSize(); i++)
			{
				auto& rb = rbs->getDataAtIdx(i);
				Entity rbOwner = rbs->getEntityAtIdx(i);
				auto& t = ecs.getComponent<Transform>(rbOwner);

				float d2 = glm::length2(position - vec2(t.position.x, t.position.y));
				if (d2 < minD2)
				{
					minD2 = d2;
					found = rbOwner;
				}
			}
			return found;
		}

		inline void impulse(ECSManager& ecs)
		{
			if (Input::GetMouseButtonDown(Input::RightClick))
			{
				if (m_loadingImpulse)
					return;

				m_loadingImpulse = true;
				m_loadStartPosition = getMousePositionInWorld(ecs);
			}

			if (Input::GetMouseButtonUp(Input::RightClick))
			{
				if (!m_loadingImpulse)
					return;

				m_loadingImpulse = false;

				auto m_rbManager = ecs.getComponentManager<RigidBody2D>();
				auto componentArray = m_rbManager->getComponentArray();

				auto transforms = ecs.getComponentArray<Transform>();

				vec2 mousePositionInWorld = getMousePositionInWorld(ecs);
				vec2 direction = (mousePositionInWorld - m_loadStartPosition);
				float dragDistance = length(direction);

				for (size_t i = 0; i < componentArray->getSize(); i++)
				{

					auto& rb = componentArray->getDataAtIdx(i);
					Entity rbOwner = componentArray->getEntityAtIdx(i);
					auto& t = transforms->getDataFromEntity(rbOwner);

					float distance = glm::length(static_cast<vec2>(t.position) - m_loadStartPosition);
					// float maxDistance = (0.5f * dragDistance);
					float maxDistance = 5.0f;
					vec2 force = 1.0f * glm::clamp(maxDistance - distance, 0.0f, 1.0f) * direction;

					rb.pendingImpulseForce += force;
					componentArray->setDirty(i, true);
				}
			}
		}

		inline void fix(ECSManager& ecs)
		{
			if (Input::GetMouseButtonDown(Input::RightClick))
			{
				Entity id = getEntityAtPosition(ecs, getMousePositionInWorld(ecs));
				if (id != INVALID_ENTITY)
				{
					auto& rb = ecs.getComponent<RigidBody2D>(id);
					rb.isFixed = !rb.isFixed;
					ecs.getComponentArray<RigidBody2D>()->setEntityDirty(id, true);
				}
			}
		}

		inline void spring(ECSManager& ecs)
		{

			if (Input::GetMouseButtonDown(Input::RightClick))
			{
				Entity id = getEntityAtPosition(ecs, getMousePositionInWorld(ecs));
				if (id != INVALID_ENTITY)
				{
					m_firstIdInSpring = id;
				}
				else
				{
					m_firstIdInSpring = INVALID_ENTITY;
				}
			}

			if (Input::GetMouseButtonUp(Input::RightClick))
			{
				if (m_firstIdInSpring != INVALID_ENTITY)
				{
					// Check
					Entity id = getEntityAtPosition(ecs, getMousePositionInWorld(ecs));
					if (id != INVALID_ENTITY)
					{
						Entity entity = ecs.createEntity();
						auto& spring = ecs.addComponent<Spring>(entity);
						spring.entityA = id;
						spring.entityB = m_firstIdInSpring;
						spring.stiffness = 0.1f;
						spring.restDistance = 1.4142f;
						ecs.getComponentArray<Spring>()->setEntityDirty(entity, true);
					}
				}

				m_firstIdInSpring = INVALID_ENTITY;
			}
		}

		inline void positionConstraint(ECSManager& ecs)
		{

			if (Input::GetMouseButtonDown(Input::RightClick))
			{
				Entity id = getEntityAtPosition(ecs, getMousePositionInWorld(ecs));
				if (id != INVALID_ENTITY)
				{
					m_firstIdInSpring = id;
				}
				else
				{
					m_firstIdInSpring = INVALID_ENTITY;
				}
			}

			if (Input::GetMouseButtonUp(Input::RightClick))
			{
				if (m_firstIdInSpring != INVALID_ENTITY)
				{
					// Check
					Entity id = getEntityAtPosition(ecs, getMousePositionInWorld(ecs));
					if (id != INVALID_ENTITY)
					{
						Entity entity = ecs.createEntity();
						auto& constraint = ecs.addComponent<DistanceConstraint>(entity);
						constraint.entityA = id;
						constraint.entityB = m_firstIdInSpring;
						constraint.distance = 1.0f; // Was default in simulation.addPositionConstraint
						ecs.getComponentArray<DistanceConstraint>()->setEntityDirty(entity, true);
					}
				}

				m_firstIdInSpring = INVALID_ENTITY;
			}
		}
	} // namespace PhysicsInteractionSystem
} // namespace WeirdEngine