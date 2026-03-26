#pragma once

#include "Entity.h"
#include "Component.h"
#include "ComponentManager.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>

namespace WeirdEngine
{
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
			for (auto const& pair : m_componentManagers) {
				const auto& manager = pair.second;
				manager->removeData(entity);
			}

			for (auto sys : m_systems) {

			}
		}

		void freeRemovedComponents() 
		{
			for (auto const& pair : m_componentManagers) 
			{
				const auto manager = pair.second;
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
			return getComponentManager<T>()->template hasComponent<T>(entity);
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

			m_componentManagers[typeid(T).name()] = pointerToManager;
		}

		template <typename T>
		void registerComponent(std::shared_ptr<ComponentManager<T>> manager) 
		{
			manager->registerComponent();
			m_componentManagers[typeid(T).name()] = manager;
		}

		template <typename T>
		std::shared_ptr<ComponentManager<T>> getComponentManager() {

			auto key = typeid(T).name();

			// Attempt to find the key in the map
			auto it = m_componentManagers.find(key);

			// Check if the key was found
			if (it == m_componentManagers.end()) {
				registerComponent<T>();
			}

			

			auto result = std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[key]);

			return result;
		}

		template <typename T>
		std::shared_ptr<ComponentArray<T>> getComponentArray()
		{
			return getComponentManager<T>()->getComponentArray();
		}

	private:
		std::unordered_map<std::string, std::shared_ptr<IComponentManager>> m_componentManagers;
		std::vector<std::shared_ptr<System>> m_systems;
		Entity m_entityCount = 0;

	};
}

#include "../../weird-physics/components/RigidBody.h"
#include "../../weird-renderer/components/Camera.h"
#include "../../weird-renderer/components/InstancedMeshRenderer.h"
#include "../../weird-renderer/components/MeshRenderer.h"
#include "../components/FlyMovement.h"
#include "../components/FlyMovement2D.h"
#include "../components/Transform.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/TextRenderer.h"

#include "../systems/InstancedRenderSystem.h"
#include "../systems/RenderSystem.h"
#include "../systems/SDFRenderSystem.h"

#include "weird-physics/components/RigidBodyManager.h"

#include "../systems/CameraSystem.h"
#include "../systems/PhysicsInteractionSystem.h"
#include "../systems/PhysicsSystem2D.h"
#include "../systems/PlayerMovementSystem.h"
#include "../systems/SDFRenderSystem2D.h"
