#pragma once
#include "../ECS.h"
#include "../../Input.h"
#include "../../../weird-physics/Simulation2D.h"


class PhysicsInteractionSystem : public System
{
private:

	bool m_holdingMouse;
	vec2 m_position;

public:

	PhysicsInteractionSystem(ECS& ecs) : m_holdingMouse(false)
	{

	}

	void update(ECS& ecs, Simulation2D& simulation)
	{
		push(ecs, simulation);
	}

private:

	void explode(ECS& ecs, Simulation2D& simulation)
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

	void push(ECS& ecs, Simulation2D& simulation)
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
				simulation.addForce(rb.simulationId, 1.f * direction);
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
};