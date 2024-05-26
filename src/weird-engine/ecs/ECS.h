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
public:
    std::vector<Entity> entities;
};

// ECS Manager
class ECS {
public:
    Entity createEntity() {
        return entityCount++;
    }

    void destroyEntity(Entity entity) {
        for (auto const& pair : componentManagers) {
            auto const& component = pair.second;
            /*if (component->hasData(entity)) {
                component->removeData(entity);
            }*/
        }
    }

    template <typename T>
    void registerComponent() {

        ComponentManager manager;
        auto pointerToManager = std::make_shared<ComponentManager>(manager);

        componentManagers[typeid(T).name()] = pointerToManager;
    }

    template <typename T>
    void addComponent(Entity entity, T component) {
        getComponentManager<T>()->addComponent(entity, component);
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
    bool hasComponent(Entity entity) {
        return getComponentManager<T>()->hasComponent<T>(entity);
    }

    template <typename T>
    void addSystem(std::shared_ptr<System> system) {
        systems.push_back(system);
    }

private:
    std::unordered_map<const char*, std::shared_ptr<ComponentManager>> componentManagers;
    std::vector<std::shared_ptr<System>> systems;
    Entity entityCount = 0;

    template <typename T>
    std::shared_ptr<ComponentManager> getComponentManager() {
        return componentManagers[typeid(T).name()];
    }
};




// Example Systems
class MovementSystem : public System {
public:
    void update(ECS& ecs) {
        for (auto entity : entities) {
            auto& t = ecs.getComponent<Transform>(entity);

            t.x += 0.1f;
        }
    }
};

