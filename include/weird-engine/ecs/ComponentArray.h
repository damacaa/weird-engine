#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>

#include "Entity.h"
#include "weird-engine/Assert.h"
#include "weird-engine/Logger.h"

namespace WeirdEngine
{
	constexpr size_t INVALID_INDEX = static_cast<size_t>(-1);

	// ComponentArray to store components of a specific type
	template <typename T> class ComponentArray
	{
	public:
		std::array<T, MAX_ENTITIES> values;
		std::array<bool, MAX_ENTITIES> dirtyFlags = []
		{
			std::array<bool, MAX_ENTITIES> arr;
			arr.fill(false);
			return arr;
		}();
		size_t size = 0;

		void insertData(Entity entity, T component)
		{
			if (entity >= MAX_ENTITIES || size >= MAX_ENTITIES)
			{
				Logger::error("[ComponentArray] Cannot insert component: capacity exceeded (" +
							  std::to_string(MAX_ENTITIES.id) + ") or entity ID (" + std::to_string(entity.id) +
							  ") is invalid.");
				return;
			}
			entityToIndexMap[entity] = size;
			indexToEntityMap[size] = entity;
			values[size] = component;
			dirtyFlags[size] = true;
			++size;
		}

		T& getNewComponent(Entity entity)
		{
			if (entity >= MAX_ENTITIES || size >= MAX_ENTITIES)
			{
				Logger::error("[ComponentArray] Cannot allocate component: capacity exceeded (" +
							  std::to_string(MAX_ENTITIES.id) + ") or entity ID (" + std::to_string(entity.id) +
							  ") is invalid.");
				static T dummy{};
				dummy = T{};
				return dummy;
			}
			entityToIndexMap[entity] = size;
			indexToEntityMap[size] = entity;
			values[size] = T{};
			dirtyFlags[size] = true;

			return values[size++];
		}

		void removeData(Entity entity)
		{
			if (!hasData(entity) || size == 0)
			{
				return;
			}
			size_t indexOfRemovedEntity = entityToIndexMap[entity];
			if (indexOfRemovedEntity >= size)
			{
				entityToIndexMap[entity] = INVALID_INDEX;
				return;
			}
			size_t indexOfLastElement = size - 1;
			values[indexOfRemovedEntity] = values[indexOfLastElement];
			dirtyFlags[indexOfRemovedEntity] = dirtyFlags[indexOfLastElement];

			Entity entityOfLastElement = indexToEntityMap[indexOfLastElement];
			entityToIndexMap[entityOfLastElement] = indexOfRemovedEntity;
			indexToEntityMap[indexOfRemovedEntity] = entityOfLastElement;
			entityToIndexMap[entity] = INVALID_INDEX;

			values[indexOfLastElement] = T{};
			dirtyFlags[indexOfLastElement] = false;

			--size;
		}

		T& getDataFromEntity(Entity entity)
		{
			if (entity >= MAX_ENTITIES || !hasData(entity))
			{
				Logger::error("[ComponentArray] Entity " + std::to_string(entity.id) +
							  " does not have component in ComponentArray.");
				static T dummy{};
				dummy = T{};
				return dummy;
			}
			return values[entityToIndexMap[entity]];
		}

		const T& getDataFromEntity(Entity entity) const
		{
			if (entity >= MAX_ENTITIES || !hasData(entity))
			{
				Logger::error("[ComponentArray] Entity " + std::to_string(entity.id) +
							  " does not have component in ComponentArray.");
				static const T dummy{};
				return dummy;
			}
			return values[entityToIndexMap[entity]];
		}

		T& getDataAtIdx(size_t idx)
		{
			if (idx >= size)
			{
				Logger::error("[ComponentArray] Index " + std::to_string(idx) + " out of range in getDataAtIdx.");
				static T dummy{};
				dummy = T{};
				return dummy;
			}
			return values[idx];
		}

		const T& getDataAtIdx(size_t idx) const
		{
			if (idx >= size)
			{
				Logger::error("[ComponentArray] Index " + std::to_string(idx) + " out of range in getDataAtIdx.");
				static const T dummy{};
				return dummy;
			}
			return values[idx];
		}

		Entity getEntityAtIdx(size_t idx) const
		{
			if (idx >= size)
				return INVALID_ENTITY;
			return indexToEntityMap[idx];
		}

		T& getLastData()
		{
			if (size == 0)
			{
				Logger::error("[ComponentArray] getLastData called on empty ComponentArray.");
				static T dummy{};
				dummy = T{};
				return dummy;
			}
			return values[size - 1];
		}

		const T& getLastData() const
		{
			if (size == 0)
			{
				Logger::error("[ComponentArray] getLastData called on empty ComponentArray.");
				static const T dummy{};
				return dummy;
			}
			return values[size - 1];
		}

		bool hasData(Entity entity) const
		{
			if (entity >= MAX_ENTITIES)
				return false;
			return entityToIndexMap[entity] != INVALID_INDEX;
		}

		void setDirty(size_t idx, bool dirty)
		{
			if (idx < size)
				dirtyFlags[idx] = dirty;
		}

		bool isDirty(size_t idx) const
		{
			if (idx < size)
				return dirtyFlags[idx];
			return false;
		}

		void setEntityDirty(Entity entity, bool dirty)
		{
			if (hasData(entity))
				dirtyFlags[entityToIndexMap[entity]] = dirty;
		}

		void setComponentDirty(const T& component, bool dirty)
		{
			ptrdiff_t diff = &component - values.data();
			if (diff >= 0 && diff < static_cast<ptrdiff_t>(size))
			{
				dirtyFlags[diff] = dirty;
			}
		}

		bool isComponentDirty(const T& component) const
		{
			ptrdiff_t diff = &component - values.data();
			if (diff >= 0 && diff < static_cast<ptrdiff_t>(size))
			{
				return dirtyFlags[diff];
			}
			return false;
		}

		Entity getEntityFromComponent(const T& component) const
		{
			ptrdiff_t diff = &component - values.data();
			if (diff >= 0 && diff < static_cast<ptrdiff_t>(size))
			{
				return indexToEntityMap[diff];
			}
			return INVALID_ENTITY;
		}

		bool isEntityDirty(Entity entity) const
		{
			if (!hasData(entity))
				return false;
			return dirtyFlags[entityToIndexMap[entity]];
		}

		// Overload [] operator for non-const objects (modifiable)
		T& operator[](unsigned int index)
		{
			if (index >= size)
			{
				Logger::error("[ComponentArray] operator[] index " + std::to_string(index) +
							  " out of range (size: " + std::to_string(size) + ").");
				static T dummy{};
				dummy = T{};
				return dummy;
			}

			return values[index];
		}

		// Function to get the size of the array
		int getSize() const
		{
			return static_cast<int>(size);
		}

	private:
		std::array<size_t, MAX_ENTITIES> entityToIndexMap = []
		{
			std::array<size_t, MAX_ENTITIES> arr;
			arr.fill(INVALID_INDEX);
			return arr;
		}();
		std::array<Entity, MAX_ENTITIES> indexToEntityMap = {};
	};

} // namespace WeirdEngine