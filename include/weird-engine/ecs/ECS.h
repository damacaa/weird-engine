#pragma once

#include "Component.h"
#include "ComponentManager.h"
#include "Entity.h"

#include <iostream>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

#pragma once
#include "ComponentManager.h"
#include "Entity.h"
#include "System.h"
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace WeirdEngine
{
	class ECSManager
	{
	public:
		Entity createEntity()
		{
			return m_entityCount++;
		}

		template <typename T>
		T& addComponent(Entity entity)
		{
			return getComponentManager<T>()->getNewComponent(entity);
		}

		template <typename T>
		T& getComponent(Entity entity)
		{
			return getComponentManager<T>()->getComponent(entity);
		}

		template <typename T>
		bool hasComponent(Entity entity) const
		{
			return getComponentManager<T>()->template hasComponent<T>(entity);
		}

		template <typename T>
		std::shared_ptr<ComponentManager<T>> getComponentManager()
		{
			const auto key = typeid(T).name();
			auto it = m_componentManagers.find(key);
			if (it == m_componentManagers.end())
			{
				registerComponent<T>();
			}
			return std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[key]);
		}

		template <typename T>
		void registerComponent()
		{
			m_componentManagers[typeid(T).name()] = std::make_shared<ComponentManager<T>>();
		}

		void addSystem(std::shared_ptr<System> system)
		{
			system->init(*this);
			system->cacheComponents(*this); // Allow systems to cache components
			m_systems.push_back(system);
		}

		void updateSystems(float dt)
		{
			for (auto& system : m_systems)
			{
				system->update(*this, dt);
			}
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<IComponentManager>> m_componentManagers;
		std::vector<std::shared_ptr<System>> m_systems;
		Entity m_entityCount = 0;
	};
}

#include "Components/Camera.h"
#include "Components/CustomShape.h"
#include "Components/FlyMovement.h"
#include "Components/FlyMovement2D.h"
#include "Components/InstancedMeshRenderer.h"
#include "Components/MeshRenderer.h"
#include "Components/RigidBody.h"
#include "Components/SDFRenderer.h"
#include "Components/Transform.h"

#include "Systems/InstancedRenderSystem.h"
#include "Systems/RenderSystem.h"
#include "Systems/SDFRenderSystem.h"

#include "Components/RigidBodyManager.h"

#include "Systems/CameraSystem.h"
#include "Systems/PhysicsInteractionSystem.h"
#include "Systems/PhysicsSystem2D.h"
#include "Systems/PlayerMovementSystem.h"
#include "Systems/SDFRenderSystem2D.h"
