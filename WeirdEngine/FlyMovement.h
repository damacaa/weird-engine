#pragma once
#include "Component.h"
class FlyMovement :	public Component
{
private:
	float m_angleX = 0;
	float m_angleY = 0;

public:

	FlyMovement(Entity* owner);

	void Update();
};

