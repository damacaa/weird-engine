#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"


class PhysicsInteractionSystem : public System
{
private:

	bool m_holdingMouse;
	vec2 m_position;
	SimulationID m_dragId = -1;

	int m_currentMaterial = 0;
	SimulationID m_lastId = -1;
	SimulationID m_selectedId = -1;
public:

	PhysicsInteractionSystem(ECSManager& ecs) : m_holdingMouse(false)
	{

	}

	void update(ECSManager& ecs, Simulation2D& simulation)
	{

		if (Input::GetMouseButton(Input::RightClick))
		{
			// Test screen coordinates to 2D world coordinates
			auto& cameraTransform = ecs.getComponent<Transform>(0);// m_mainCamera

			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			vec2 pp = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			//std::cout << "Click: " << pp.x << ", " << pp.y << std::endl;

			SimulationID id = simulation.raycast(pp);
			if (m_dragId >= simulation.getSize() && id < simulation.getSize())
				m_dragId = id;

			if (m_dragId < simulation.getSize())
			{
				drag(ecs, simulation, m_dragId, pp);
			}
			else
			{
				push(ecs, simulation);
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

			release(ecs, simulation);
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



		if (Input::GetMouseButtonDown(Input::LeftClick))
		{
			// Test screen coordinates to 2D world coordinates
			auto& cameraTransform = ecs.getComponent<Transform>(0);

			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			vec2 pp = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			//std::cout << "Click: " << pp.x << ", " << pp.y << std::endl;


			if (!Input::GetKey(Input::Z))
			{
				Transform t;
				//t.position = vec3(pp.x + sin(time), pp.y + cos(time), 0.0);
				t.position = vec3(pp.x, pp.y, 0.0);
				Entity entity = ecs.createEntity();
				ecs.addComponent(entity, t);

				ecs.addComponent(entity, SDFRenderer(m_currentMaterial + 4));

				RigidBody2D rb(simulation);
				ecs.addComponent(entity, rb);
				simulation.addForce(rb.simulationId, 1000.0f * vec2(Input::GetMouseDeltaX(), -Input::GetMouseDeltaY()));

				m_lastId = -1;
			}
			else
			{
				SimulationID id = simulation.raycast(pp);
				if (id < simulation.getSize())
				{
					//simulation.fix(id);
					if (m_lastId < simulation.getSize())
					{
						simulation.addSpring(id, m_lastId, 10000000000.0f);
						m_lastId = -1;
					}
					else
					{
						m_lastId = id;
					}
				}
				else
				{
					std::cout << "Miss" << std::endl;
					m_lastId = -1;
				}
			}
		}

		if (Input::GetMouseButtonUp(Input::LeftClick))
		{
			m_currentMaterial = (m_currentMaterial + 1) % 12;
		}

		if (Input::GetKeyDown(Input::X))
		{
			std::cout << "Reset spring" << std::endl;
			m_lastId = -1;
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

	void push(ECSManager& ecs, Simulation2D& simulation)
	{
		if (m_holdingMouse && Input::GetMouseButtonUp(Input::RightClick))
		{
			m_holdingMouse = false;

			auto& m_rbManager = *ecs.getComponentManager<RigidBody2D>();
			auto& componentArray = *m_rbManager.getComponentArray<RigidBody2D>();

			float x = Input::GetMouseX() / 40.0f;
			float y = (800 - Input::GetMouseY()) / 40.0f;
			vec2 direction = (vec2(x, y) - m_position);

			for (size_t i = 0; i < componentArray.size; i++)
			{
				auto& rb = componentArray[i];
				simulation.addForce(rb.simulationId, 3.f * direction);
			}
		}
		else if (Input::GetMouseButtonDown(Input::RightClick))
		{
			m_holdingMouse = true;

			float x = Input::GetMouseX() / 40.0f;
			float y = (800 - Input::GetMouseY()) / 40.0f;
			m_position = vec2(x, y);
		}
	}

	void release(ECSManager& ecs, Simulation2D& simulation)
	{
		if (!m_holdingMouse)
			return;

		m_holdingMouse = false;

		auto& m_rbManager = *ecs.getComponentManager<RigidBody2D>();
		auto& componentArray = *m_rbManager.getComponentArray<RigidBody2D>();

		float x = Input::GetMouseX() / 40.0f;
		float y = (800 - Input::GetMouseY()) / 40.0f;
		vec2 direction = (vec2(x, y) - m_position);

		for (size_t i = 0; i < componentArray.size; i++)
		{
			auto& rb = componentArray[i];
			simulation.addForce(rb.simulationId, 3.f * direction);
		}

	}

	void drag(ECSManager& ecs, Simulation2D& simulation, SimulationID id, vec2 pp) {

		simulation.setPosition(id, pp);
		simulation.fix(id);
	}
};