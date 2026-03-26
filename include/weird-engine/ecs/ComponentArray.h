#pragma once

#include <array>
#include <unordered_map>
#include <stdexcept>

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
            entityToIndexMap[entity] = size;
            indexToEntityMap[size] = entity;
            values[size] = component;
            ++size;
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

        Entity getEntityAtIdx(size_t idx) const
        {
            return indexToEntityMap[idx];
        }

        T& getLastData()
        {
            return values[size - 1];
        }

        bool hasData(Entity entity)
        {
            return entityToIndexMap[entity] < size;
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
        std::array<size_t, MAX_ENTITIES> entityToIndexMap = {};
        std::array<Entity, MAX_ENTITIES> indexToEntityMap = {};
    };

}