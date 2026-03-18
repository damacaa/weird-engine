#pragma once

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <limits>
#include <utility>

#include "Entity.h"
#include "Component.h"

namespace WeirdEngine
{
    // Constant representing an invalid/empty index
    constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

    template <typename T>
    class ComponentArray
    {
    public:
        ComponentArray()
        {
            // The sparse map must be large enough to hold any Entity ID.
            // We initialize all values to INVALID_INDEX to denote "no component".
            entityToIndexMap.resize(MAX_ENTITIES, INVALID_INDEX);

            // Optional: reserve a little bit of space for the packed arrays to avoid early reallocations
            values.reserve(128);
            indexToEntityMap.reserve(128);
        }

        void insertData(Entity entity, T component)
        {
            if (hasData(entity)) {
                // Prevent duplicate insertions
                values[entityToIndexMap[entity]] = std::move(component);
                return;
            }

            size_t newIndex = values.size();
            entityToIndexMap[entity] = newIndex;
            indexToEntityMap.push_back(entity);

            // push_back actually adds to the size dynamically
            values.push_back(std::move(component));
        }

        T& getNewComponent(Entity entity)
        {
            size_t newIndex = values.size();
            entityToIndexMap[entity] = newIndex;
            indexToEntityMap.push_back(entity);

            // emplace_back constructs the object directly inside the vector
            values.emplace_back();
            return values.back();
        }

        void removeData(Entity entity)
        {
            if (!hasData(entity)) return;

            size_t indexOfRemovedEntity = entityToIndexMap[entity];
            size_t indexOfLastElement = values.size() - 1;

            // 1. Move the last component into the deleted slot (std::move avoids expensive copying)
            values[indexOfRemovedEntity] = std::move(values[indexOfLastElement]);

            // 2. Update the map to point to the moved entity
            Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
            entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
            indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;

            // 3. Mark the removed entity's index as invalid
            entityToIndexMap[entity] = INVALID_INDEX;

            // 4. Shrink the packed arrays by removing the last elements
            values.pop_back();
            indexToEntityMap.pop_back();
        }

        T& getDataFromEntity(Entity entity)
        {
            return values[entityToIndexMap[entity]];
        }

        T& getDataAtIdx(size_t idx)
        {
            return values[idx];
        }

        T& getLastData()
        {
            return values.back();
        }

        bool hasData(Entity entity) const
        {
            // Fast and safe check
            return entityToIndexMap[entity] != INVALID_INDEX;
        }

        T& operator[](size_t index)
        {
            if (index >= values.size())
            {
                throw std::out_of_range("Index out of range");
            }
            return values[index];
        }

        size_t getSize() const
        {
            return values.size();
        }

    private:
        // Packed arrays: Size strictly equals the number of active components
        std::vector<T> values;
        std::vector<Entity> indexToEntityMap;

        // Sparse array: Size is always MAX_ENTITIES
        std::vector<size_t> entityToIndexMap;
    };
}