#pragma once

#include "Entity.h"
#include "Component.h"
#include "ComponentManager.h"

#include <iostream>
#include <vector>
#include <unordered_map>
#include <typeindex>
#include <memory>


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

	void destroyEntity(Entity entity) {
		for (auto const& pair : m_componentManagers) {
			const std::shared_ptr<ComponentManager>& manager = pair.second;
			manager->removeData(entity);
		}

		for (auto sys : m_systems) {
			
		}
	}

	template <typename T>
	void addComponent(Entity entity, T component) {
		auto cm = getComponentManager<T>();
		component.Owner = entity;
		cm->addComponent(entity, component);
	}

	template <typename T>
	void removeComponent(Entity entity) {
		getComponentManager<T>()->removeComponent(entity);
	}

	template <typename T>
	T& getComponent(Entity entity) {
		return getComponentManager<T>()->getComponent<T>(entity);
	}

	template <typename T>
	bool hasComponent(Entity entity) const {
		return getComponentManager<T>()->hasComponent<T>(entity);
	}

	template <typename T>
	void addSystem(std::shared_ptr<System> system) {
		m_systems.push_back(system);
	}

	template <typename T>
	void registerComponent() {

		ComponentManager manager;
		manager.registerComponent<T>();
		auto pointerToManager = std::make_shared<ComponentManager>(manager);

		m_componentManagers[typeid(T).name()] = pointerToManager;
	}

	template <typename T>
	std::shared_ptr<ComponentManager> getComponentManager() {

		auto key = typeid(T).name();

		// Attempt to find the key in the map
		auto it = m_componentManagers.find(key);

		// Check if the key was found
		if (it == m_componentManagers.end()) {
			registerComponent<T>();
		}

		return m_componentManagers[key];
	}

	template <typename T>
	std::shared_ptr<ComponentArray<T>> getComponentArray()
	{
		return getComponentManager<T>()->getComponentArray<T>();
	}

private:
	std::unordered_map<const char*, std::shared_ptr<ComponentManager>> m_componentManagers;
	std::vector<std::shared_ptr<System>> m_systems;
	Entity m_entityCount = 0;

};

#include "Components/Transform.h"
#include "Components/SDFRenderer.h"
#include "Components/MeshRenderer.h"
#include "Components/InstancedMeshRenderer.h"
#include "Components/RigidBody.h"
#include "Components/Camera.h"
#include "Components/FlyMovement.h"
#include "Components/FlyMovement2D.h"
#include "Components/CustomShape.h"

#include "Systems/RenderSystem.h"
#include "Systems/InstancedRenderSystem.h"
#include "Systems/SDFRenderSystem.h"



#include "Systems/SDFRenderSystem2D.h"
#include "Systems/PhysicsSystem2D.h"
#include "Systems/PhysicsInteractionSystem.h"
#include "Systems/PlayerMovementSystem.h"
#include "Systems/CameraSystem.h"
#include "Systems/DebugSystems.h"


