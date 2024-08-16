#pragma once
#include "../Component.h"

struct SDFRenderer : public Component {
	bool isStatic = false;
	unsigned int materialId;
};

