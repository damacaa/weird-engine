#pragma once
#include "../Component.h"
#include "../../math/MathExpressions.h"


struct CustomShape : public Component 
{
public:

	uint16_t id = -1;
	uint16_t m_distanceFieldId;
	float m_parameters[8];
	bool m_isDirty;

	CustomShape() : m_distanceFieldId(0), m_isDirty(true)
	{
	}

	CustomShape(uint16_t id, float* params) : m_distanceFieldId(id), m_isDirty(true)
	{
		std::copy(params, params + 8, m_parameters);
	}

};