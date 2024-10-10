#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"


class PhysicsInteractionSystem : public System
{
private:

	bool m_loadingImpulse;
	vec2 m_loadStartPosition;

	SimulationID m_dragId = -1;

	int m_currentMaterial = 0;

	SimulationID m_firstIdInSpring = -1;

	SimulationID m_selectedId = -1;

	enum class InteractionMode
	{
		Drag = 0,
		Impulse = 1,
		Fix = 2,
		Spring = 3,
	};

	std::string m_interactionModeToString[4]
	{
		"Drag",
		"Impulse",
		"Fix",
		"Spring",
	};

	InteractionMode m_currentInteractionMode = InteractionMode::Spring;

	vec2 getMousePositionInWorld(ECSManager& ecs, Simulation2D& simulation)
	{
		// Get mouse coordinates
		auto& cameraTransform = ecs.getComponent<Transform>(0);// m_mainCamera
		float x = Input::GetMouseX();
		float y = Input::GetMouseY();

		// Transform mouse coordinates to world space
		vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

		return mousePositionInWorld;
	}



	void drag(ECSManager& ecs, Simulation2D& simulation)
	{
		if (Input::GetMouseButtonDown(Input::RightClick))
		{
			// Find new target
			SimulationID id = simulation.raycast(getMousePositionInWorld(ecs, simulation));

			// No prior target and new target is good
			if (!m_dragId < simulation.getSize() && id < simulation.getSize())
			{
				m_dragId = id;
				simulation.fix(m_dragId);
			}
		}

		if (Input::GetMouseButtonUp(Input::RightClick))
		{
			if (m_dragId < simulation.getSize())
			{
				simulation.unFix(m_dragId);
				simulation.addForce(m_dragId, 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY()));
				m_dragId = -1;
			}
		}

		if (m_dragId < simulation.getSize())
		{
			vec2 mousePositionInWorld = getMousePositionInWorld(ecs, simulation);
			simulation.setPosition(m_dragId, mousePositionInWorld);
		}
	}


	void impulse(ECSManager& ecs, Simulation2D& simulation)
	{
		if (Input::GetMouseButtonDown(Input::RightClick))
		{
			if (m_loadingImpulse)
				return;

			m_loadingImpulse = true;
			m_loadStartPosition = getMousePositionInWorld(ecs, simulation);
		}


		if (Input::GetMouseButtonUp(Input::RightClick))
		{
			if (!m_loadingImpulse)
				return;

			m_loadingImpulse = false;

			auto& m_rbManager = *ecs.getComponentManager<RigidBody2D>();
			auto& componentArray = *m_rbManager.getComponentArray<RigidBody2D>();

			vec2 direction = (getMousePositionInWorld(ecs, simulation) - m_loadStartPosition);

			for (size_t i = 0; i < componentArray.size; i++)
			{
				auto& rb = componentArray[i];
				simulation.addForce(rb.simulationId, direction);
			}
		}
	}


	void fix(ECSManager& ecs, Simulation2D& simulation)
	{
		if (Input::GetMouseButtonDown(Input::RightClick))
		{
			SimulationID id = simulation.raycast(getMousePositionInWorld(ecs, simulation));
			if (id < simulation.getSize())
			{
				simulation.fix(id);
			}
		}
	}


	void spring(ECSManager& ecs, Simulation2D& simulation)
	{

		if (Input::GetMouseButtonDown(Input::RightClick))
		{
			SimulationID id = simulation.raycast(getMousePositionInWorld(ecs, simulation));
			if (id < simulation.getSize())
			{
				m_firstIdInSpring = id;
			}
			else
			{
				m_firstIdInSpring = -1;
			}
		}


		if (Input::GetMouseButtonUp(Input::RightClick))
		{
			if (m_firstIdInSpring < simulation.getSize())
			{
				// Check 
				SimulationID id = simulation.raycast(getMousePositionInWorld(ecs, simulation));
				if (id < simulation.getSize())
				{
					simulation.addSpring(id, m_firstIdInSpring, 10000000000.0f);
				}
			}

			m_firstIdInSpring = -1;
		}
	}

public:

	PhysicsInteractionSystem(ECSManager& ecs) : m_loadingImpulse(false)
	{

	}

	void update(ECSManager& ecs, Simulation2D& simulation)
	{

		// Spawn ball
		if (Input::GetMouseButtonDown(Input::LeftClick))
		{
			// Get mouse coordinates world space
			vec2 mousePositionInWorld = getMousePositionInWorld(ecs, simulation);

			Transform t;
			//t.position = vec3(mousePositionInWorld.x + sin(time), mousePositionInWorld.y + cos(time), 0.0);
			t.position = vec3(mousePositionInWorld.x, mousePositionInWorld.y, 0.0);
			Entity entity = ecs.createEntity();
			ecs.addComponent(entity, t);

			ecs.addComponent(entity, SDFRenderer(m_currentMaterial + 4));

			RigidBody2D rb(simulation);
			ecs.addComponent(entity, rb);
			simulation.addForce(rb.simulationId, 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY()));

			m_firstIdInSpring = -1;
		}

		switch (m_currentInteractionMode)
		{
		case PhysicsInteractionSystem::InteractionMode::Drag:
			drag(ecs, simulation);
			break;
		case PhysicsInteractionSystem::InteractionMode::Impulse:
			impulse(ecs, simulation);
			break;
		case PhysicsInteractionSystem::InteractionMode::Fix:
			fix(ecs, simulation);
			break;
		case PhysicsInteractionSystem::InteractionMode::Spring:
			spring(ecs, simulation);
			break;
		default:
			break;
		}

		

		// Add force to last ball
		if (Input::GetKey(Input::T))
		{
			SimulationID lastInSimulation = simulation.getSize() - 1;

			int last = ecs.getComponentArray<RigidBody2D>()->getSize() - 1;
			SimulationID target = ecs.getComponentArray<RigidBody2D>()->getDataAtIdx(last).simulationId;

			auto v = vec2(15, 30) - simulation.getPosition(target);
			simulation.addForce(target, 50.0f * normalize(v));
		}



		if (Input::GetMouseButtonUp(Input::LeftClick))
		{
			m_currentMaterial = (m_currentMaterial + 1) % 12;
		}


		if (Input::GetKeyDown(Input::M))
		{

			std::cout << "Choose interaction" << std::endl;

			for (size_t i = 0; i < m_interactionModeToString->size(); i++)
			{
				std::cout << i << "-->" << m_interactionModeToString[i] << std::endl;
			}

			std::string input;
			std::cin >> input;
			int32_t newMode = stoi(input);
			if (newMode >= 0 && newMode < m_interactionModeToString->size())
			{
				m_currentInteractionMode = (InteractionMode)newMode;
				std::cout << m_interactionModeToString[(int)m_currentInteractionMode] << std::endl;
			}
		}


		// Pause / resume simulation
		if (Input::GetKeyDown(Input::P))
		{
			if (simulation.isPaused())
			{
				simulation.resume();
			}
			else
			{
				simulation.pause();
			}
		}

	}

private:

	void explode(ECSManager& ecs, Simulation2D& simulation)
	{
		if (Input::GetMouseButtonDown(Input::RightClick))
		{
			auto& m_rbManager = *ecs.getComponentManager<RigidBody2D>();
			auto& componentArray = *m_rbManager.getComponentArray<RigidBody2D>();

			// TODO: use screen size and camera position
			float x = Input::GetMouseX() / 40.0f;
			float y = (800 - Input::GetMouseY()) / 40.0f;
			vec2 camPos = vec2(x, y);

			for (size_t i = 0; i < componentArray.size; i++)
			{
				auto& rb = componentArray[i];
				simulation.addForce(rb.simulationId, -10.f * normalize(camPos - simulation.getPosition(rb.simulationId)));
			}
		}
	}




};