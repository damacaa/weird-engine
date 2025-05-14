#pragma once
#include "ComponentManager.h"
#include "Entity.h"
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace WeirdEngine
{

	class System
	{
	public:
		virtual ~System() = default;

		// Lifecycle
		virtual void init(class ECSManager& ecs) { }
		virtual void update(class ECSManager& ecs, float dt) = 0;
		virtual void shutdown(class ECSManager& ecs) { }

		// Optional component caching
		virtual void cacheComponents(class ECSManager& ecs) { }
	};

}
