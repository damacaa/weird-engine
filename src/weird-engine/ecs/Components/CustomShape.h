#pragma once
#include "../ECS.h"
#include "../../../weird-physics/Simulation.h"
#include "../../MathExpression.h"

struct RigidBody : public Component {
private:
	std::shared_ptr<IMathExpression> m_sdf;
public:

	RigidBody() : simulationId(-1) {};

	unsigned int simulationId;

};