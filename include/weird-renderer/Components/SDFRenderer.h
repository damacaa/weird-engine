#pragma once
#include "weird-engine/ecs/Component.h"

namespace WeirdEngine
{
	struct SDFRenderer : public Component
	{
		bool isStatic = false;
		unsigned int materialId;

		SDFRenderer(unsigned int materialId)
		{
			this->materialId = materialId;
		}

		SDFRenderer()
		{
			this->materialId = 0;
		}
	};

	struct UIDot : public SDFRenderer{};
}
