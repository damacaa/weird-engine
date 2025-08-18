#pragma once

#include <weird-engine.h>

using namespace WeirdEngine;
class MouseCollisionScene : public Scene
{
public:
	MouseCollisionScene()
		: Scene() {
	};

private:
	Entity m_cursorShape;

	// Inherited via Scene
	void onStart() override
	{
		for (size_t i = 0; i < 600; i++)
		{

			float y = (int)(i / 20);
			float x = 5 + (i % 20) + sin(y);

			int material = 4 + (i % 12);

			float z = 0;

			Entity entity = m_ecs.createEntity();
			Transform& t = m_ecs.addComponent<Transform>(entity);
			t.position = vec3(x + 0.5f, y + 0.5f, z);

			if (i < 100)
			{
			}

			SDFRenderer& sdfRenderer = m_ecs.addComponent<SDFRenderer>(entity);
			sdfRenderer.materialId = material;

			RigidBody2D& rb = m_ecs.addComponent<RigidBody2D>(entity);
		}

		// Floor
		{
			float variables[8]{ 0.5f, 1.5f, -1.0f };
			addShape(0, variables, 3);
		}

		{
			float variables[8]{ -15.0f, 50.0f, 5.0f, 5.0f, 2.0f, 10.0f };
			Entity star = addShape(1, variables, 3);

			m_cursorShape = star;
		}
	}

	void onUpdate(float delta) override
	{
		// Move wall to mouse
		{
			CustomShape& cs = m_ecs.getComponent<CustomShape>(m_cursorShape);
			auto& cameraTransform = m_ecs.getComponent<Transform>(m_mainCamera);
			float x = Input::GetMouseX();
			float y = Input::GetMouseY();

			// Transform mouse coordinates to world space
			vec2 mousePositionInWorld = ECS::Camera::screenPositionToWorldPosition2D(cameraTransform, vec2(x, y));

			cs.m_parameters[0] = mousePositionInWorld.x;
			cs.m_parameters[1] = mousePositionInWorld.y;
			cs.m_isDirty = true;
		}
	}
};
