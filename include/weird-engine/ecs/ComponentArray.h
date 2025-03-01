#pragma once

#include "Entity.h"
#include "Component.h"

namespace WeirdEngine
{

		// ComponentArray to store components of a specific type
		template <typename T>
		class ComponentArray
		{
		public:
			std::array<T, MAX_ENTITIES> values;
			size_t size = 0;

			void insertData(Entity entity, T component)
			{
				entityToIndexMap[entity] = size; // ERROR: this is null
				indexToEntityMap[size] = entity;
				values[size] = component;
				++size;
			}

			void replaceData(Entity entity, Entity target)
			{
				// Find target position
				size_t idx = entityToIndexMap[target];

				// Move origin values to target
				values[idx] = values[entityToIndexMap[entity]];

				// Update maps
				entityToIndexMap[entity] = idx;
				indexToEntityMap[idx] = entity;
			}

			T& getNewComponent(Entity entity)
			{
				entityToIndexMap[entity] = size;
				indexToEntityMap[size] = entity;

				return values[size++];
			}

			void removeData(Entity entity)
			{
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

			T& getDataFromEntity(Entity entity)
			{
				return values[entityToIndexMap[entity]];
			}

			T& getDataAtIdx(size_t idx)
			{
				return values[idx];
			}

			bool hasData(Entity entity)
			{
				return entityToIndexMap.size() > 0 && entityToIndexMap.find(entity) != entityToIndexMap.end();
			}

			// Overload [] operator for non-const objects (modifiable)
			T& operator[](unsigned int index)
			{

				if (index < 0 || index >= size)
				{
					throw std::out_of_range("Index out of range");
				}

				return values[index];
			}

			// Function to get the size of the array
			int getSize() const
			{
				return size;
			}

		private:
			std::unordered_map<Entity, size_t> entityToIndexMap;
			std::unordered_map<size_t, Entity> indexToEntityMap;
		};
	
}