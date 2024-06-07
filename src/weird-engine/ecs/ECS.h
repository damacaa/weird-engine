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
	std::shared_ptr<ComponentManager> m_manager;
	std::vector<Entity> m_entities;

public:
	virtual void add(Entity entity) {
		m_entities.push_back(entity);
	}
	
	virtual void remove(Entity entity) {
		m_entities.erase(std::remove(m_entities.begin(), m_entities.end(), entity), m_entities.end());
	}

	void SetManager(std::shared_ptr<ComponentManager> manager) {
		m_manager = manager;
	}

protected:
	template<typename T>
	ComponentArray<T>& GetManagerArray() {
		return *m_manager->getComponentArray<T>();
	}

	template<typename T>
	std::shared_ptr<ComponentArray<T>> GetManagerArrayPtr() {
		return m_manager->getComponentArray<T>();
	}
};

// ECS Manager
class ECS {
public:
	Entity createEntity() {
		return m_entityCount++;
	}


	void destroyEntity(Entity entity) {
		for (auto const& pair : m_componentManagers) {
			const std::shared_ptr<ComponentManager>& component = pair.second;
			// TODO:
			component->removeData(entity);
		}

		for (auto sys : m_systems) {
			sys->remove(entity);
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
	void registerComponent(std::shared_ptr<System> system) {

		addSystem<T>(system);

		ComponentManager manager;
		manager.registerComponent<T>();
		auto pointerToManager = std::make_shared<ComponentManager>(manager);

		m_componentManagers[typeid(T).name()] = pointerToManager;

		system->SetManager(pointerToManager);
	}

	template <typename T>
	void registerComponent(System& system) {

		std::shared_ptr<System> pointerToSystem = std::make_shared<System>(system);

		addSystem<T>(pointerToSystem);

		ComponentManager manager;
		manager.registerComponent<T>();
		auto pointerToManager = std::make_shared<ComponentManager>(manager);

		m_componentManagers[typeid(T).name()] = pointerToManager;


		system.SetManager(pointerToManager);
	}

public:
	template <typename T>
	std::shared_ptr<ComponentManager> getComponentManager() {
		return m_componentManagers[typeid(T).name()];
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

#include "Systems/RenderSystem.h"
#include "Systems/InstancedRenderSystem.h"
#include "Systems/SDFRenderSystem.h"



