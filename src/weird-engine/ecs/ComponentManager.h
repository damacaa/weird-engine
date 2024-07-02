#pragma once

#include "Entity.h"
#include "Component.h"

#include "Components/Transform.h"
#include <queue>

// ComponentArray to store components of a specific type
template <typename T>
class ComponentArray {
public:

	std::array<T, MAX_ENTITIES> values;
	size_t size = 0;

	void insertData(Entity entity, T component) {
		entityToIndexMap[entity] = size; // ERROR: this is null
		indexToEntityMap[size] = entity;
		values[size] = component;
		++size;
	}

	void removeData(Entity entity) {
		size_t indexOfRemovedEntity = entityToIndexMap[entity];
		size_t indexOfLastElement = size - 1;
		values[indexOfRemovedEntity] = values[indexOfLastElement];

		Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
		entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
		indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

		entityToIndexMap.erase(entity);
		indexToEntityMap.erase(indexOfLastElement);

		--size;
	}

	T& getData(Entity entity) {
		return values[entityToIndexMap[entity]];
	}

	bool hasData(Entity entity) {
		return entityToIndexMap.size() > 0 && entityToIndexMap.find(entity) != entityToIndexMap.end();
	}


	// Overload [] operator for non-const objects (modifiable)
	T& operator[](unsigned int index) {

		if (index < 0 || index >= size) {
			throw std::out_of_range("Index out of range");
		}

		return values[index];
	}



	// Function to get the size of the array
	int getSize() const {
		return size;
	}


private:
	std::unordered_map<Entity, size_t> entityToIndexMap;
	std::unordered_map<size_t, Entity> indexToEntityMap;
};




class ComponentManager {
private:

	std::shared_ptr<void> m_componentArray;
	std::queue<Entity> m_removedEntities;

public:

	ComponentManager() {

	}

	template <typename T>
	void registerComponent() {
		m_componentArray = std::make_shared<ComponentArray<T>>();
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

	void removeData(Entity entity) {
		m_removedEntities.push(entity);
	}


	template <typename T>
	std::shared_ptr<ComponentArray<T>> getComponentArray() {

		auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);

		while (m_removedEntities.size() > 0)
		{
			Entity e = m_removedEntities.front();

			if (componentArray->hasData(e)) {
				componentArray->removeData(e);
			}

			m_removedEntities.pop();
		}

		return componentArray;

	}
};