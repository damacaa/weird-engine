#pragma once

#include "Entity.h"
#include "Component.h"
#include "ComponentManager.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>
#include <cstddef>

namespace WeirdEngine
{
	namespace internal
	{
		inline size_t nextComponentTypeId()
		{
			static size_t id = 0;
			return id++;
		}

		template <typename T>
		size_t getComponentTypeId()
		{
			// Each component type gets a unique ID at compile time
			static const size_t id = nextComponentTypeId();
			return id;
		}
	}

	// System base class
	class System {
	protected:

	public:


	};

	// ECSManager Manager
	class ECSManager {
	public:

		Entity createEntity() {
			return m_entityCount++;
		}

		Entity getEntityCount() const {
			return m_entityCount;
		}

		void destroyEntity(Entity entity) {
			for (auto const& manager : m_componentManagers) {
				if (manager)
					manager->removeData(entity);
			}

			for (auto sys : m_systems) {

			}
		}

		void freeRemovedComponents() 
		{
			for (auto const& manager : m_componentManagers) 
			{
				if (manager)
					manager->freeRemovedComponents();
			}
		}

		template <typename T>
		T& addComponent(Entity entity)
		{
			static_assert(std::is_base_of<Component, T>::value, "T must derive from Component");

			auto cm = getComponentManager<T>();
			auto& component = cm->getNewComponent(entity);

			return component;
		}

		template <typename T>
		T& getComponent(Entity entity) 
		{
			return getComponentManager<T>()->getComponent(entity);
		}

		template <typename T>
		bool hasComponent(Entity entity) const {
			size_t id = internal::getComponentTypeId<T>();
			if (id >= m_componentManagers.size() || !m_componentManagers[id])
				return false;
			return std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id])->hasComponent(entity);
		}

		template <typename T>
		void addSystem(std::shared_ptr<System> system) {
			m_systems.push_back(system);
		}

		template <typename T>
		void registerComponent() {

			ComponentManager<T> manager;
			manager.registerComponent();
			auto pointerToManager = std::make_shared<ComponentManager<T>>(manager);

			size_t id = internal::getComponentTypeId<T>();
			if (id >= m_componentManagers.size())
				m_componentManagers.resize(id + 1);
			m_componentManagers[id] = pointerToManager;
		}

		template <typename T>
		void registerComponent(std::shared_ptr<ComponentManager<T>> manager) 
		{
			size_t id = internal::getComponentTypeId<T>();
			if (id < m_componentManagers.size() && m_componentManagers[id]) {
				// A default manager already exists (e.g. cached by a system).
				// Adopt its ComponentArray so both the cached and new manager
				// share the same data.
				auto existing = std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id]);
				manager->adoptComponentArray(existing);
			} else {
				manager->registerComponent();
			}
			if (id >= m_componentManagers.size())
				m_componentManagers.resize(id + 1);
			m_componentManagers[id] = manager;
		}

		template <typename T>
		std::shared_ptr<ComponentManager<T>> getComponentManager() {

			size_t id = internal::getComponentTypeId<T>();

			if (id >= m_componentManagers.size() || !m_componentManagers[id]) {
				registerComponent<T>();
			}

			return std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id]);
		}

		template <typename T>
		std::shared_ptr<ComponentArray<T>> getComponentArray()
		{
			return getComponentManager<T>()->getComponentArray();
		}

	private:
		std::vector<std::shared_ptr<IComponentManager>> m_componentManagers;
		std::vector<std::shared_ptr<System>> m_systems;
		Entity m_entityCount = 0;

	};
}

#include "weird-physics/components/RigidBody.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/InstancedMeshRenderer.h"
#include "weird-renderer/components/MeshRenderer.h"
#include "weird-engine/components/FlyMovement.h"
#include "weird-engine/components/FlyMovement2D.h"
#include "weird-engine/components/Transform.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/TextRenderer.h"

#include "../systems/InstancedRenderSystem.h"
#include "../systems/RenderSystem.h"
#include "../systems/SDFRenderSystem.h"

#include "weird-physics/components/RigidBodyManager.h"

#include "../systems/ButtonSystem.h"
#include "../systems/CameraSystem.h"
#include "../systems/PhysicsInteractionSystem.h"
#include "../systems/PhysicsSystem2D.h"
#include "../systems/PlayerMovementSystem.h"
#include "../systems/SDFRenderSystem2D.h"
