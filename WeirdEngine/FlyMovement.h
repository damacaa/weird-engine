#pragma once
#include "Component.h"
class FlyMovement :	public Component
{
private:
	float m_angleX = 0;
	float m_angleY = 0;
	float m_speed = 10000.0f;
public:

	FlyMovement(Entity* owner);

	void Update();
};

