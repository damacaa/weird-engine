#pragma once

#include "Entity.h"
#include "Component.h"
#include "Components/Transform.h"

// ComponentArray to store components of a specific type
template <typename T>
class ComponentArray {
public:

	void insertData(Entity entity, T component) {
		entityToIndexMap[entity] = size; // ERROR: this is null
		indexToEntityMap[size] = entity;
		componentArray[size] = component;
		++size;
	}

	void removeData(Entity entity) {
		size_t indexOfRemovedEntity = entityToIndexMap[entity];
		size_t indexOfLastElement = size - 1;
		componentArray[indexOfRemovedEntity] = componentArray[indexOfLastElement];

		Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
		entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
		indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

		entityToIndexMap.erase(entity);
		indexToEntityMap.erase(indexOfLastElement);

		--size;
	}

	T& getData(Entity entity) {
		return componentArray[entityToIndexMap[entity]];
	}

	bool hasData(Entity entity) {
		return entityToIndexMap.find(entity) != entityToIndexMap.end();
	}

	float Test = 1234;

private:
	std::array<T, MAX_ENTITIES> componentArray;
	std::unordered_map<Entity, size_t> entityToIndexMap;
	std::unordered_map<size_t, Entity> indexToEntityMap;
	size_t size = 0;
};






class ComponentManager {
public:
	template <typename T>
	void registerComponent() {
		const char* typeName = typeid(T).name();
		componentArrays[typeName] = std::make_shared<ComponentArray<T>>();

		componentArray = std::make_shared<ComponentArray<T>>();
	}

	template <typename T>
	void addComponent(Entity entity, T component) {
		auto castedComponentArray = getComponentArray<T>();
		castedComponentArray->insertData(entity, component);
	}

	template <typename T>
	void removeComponent(Entity entity) {
		getComponentArray<T>()->removeData(entity);
	}

	template <typename T>
	T& getComponent(Entity entity) {
		return getComponentArray<T>()->getData(entity);
	}

	template <typename T>
	bool hasComponent(Entity entity) {
		return getComponentArray<T>()->hasData(entity);
	}


	ComponentManager() {

	}

private:
	std::unordered_map<const char*, std::shared_ptr<void>> componentArrays;
	std::shared_ptr<void> componentArray;

	template <typename T>
	std::shared_ptr<ComponentArray<T>> getComponentArray() {
		const char* typeName = typeid(T).name();
		auto it = componentArrays.find(typeName);
		if (it != componentArrays.end()) {
			return std::static_pointer_cast<ComponentArray<T>>(it->second);
		}
		else {
			return nullptr; // Or handle this case as appropriate for your application
		}
		//return std::static_pointer_cast<ComponentArray<T>>(componentArray);
	}
};