#pragma once
#include "weird-engine/ecs/Component.h"

namespace WeirdEngine
{
	struct Dot : public Component
	{
		bool isStatic = false;
		unsigned int materialId;

		Dot()
		{
			this->materialId = 0;
		}
	};

	struct UIDot : public Component
	{
		bool isStatic = false;
		unsigned int materialId;

		UIDot()
		{
			this->materialId = 0;
		}
	};
}
