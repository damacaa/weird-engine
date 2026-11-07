#pragma once

#include <istream>
#include <memory>
#include <ostream>
#include <queue>
#include <type_traits>

#include "ComponentArray.h"
#include "Entity.h"
#include "weird-engine/Logger.h"

namespace WeirdEngine
{
	class IComponentManager
	{
	public:
		virtual ~IComponentManager() = default;
		virtual void freeRemovedComponents() = 0;
		virtual void removeData(Entity entity) = 0;
		virtual bool hasComponent(Entity entity) = 0;
		virtual void clear() = 0;
		virtual size_t getElementSize() const = 0;
		virtual bool isSnapshotSerializable() const = 0;
		virtual void serializeBinary(std::ostream& os) const = 0;
		virtual bool deserializeBinary(std::istream& is, size_t fileElemSize) = 0;
		virtual std::shared_ptr<void> createEmptyArray() = 0;
		virtual std::shared_ptr<void> getArray() const = 0;
		virtual void setArray(std::shared_ptr<void> arr) = 0;
		virtual bool deserializeToArray(std::istream& is, size_t fileElemSize, std::shared_ptr<void>& outArray) = 0;
	};

	template <typename T> class ComponentManager : public IComponentManager
	{
	protected:
		std::shared_ptr<void> m_componentArray;
		std::queue<Entity> m_removedEntities;

		virtual void handleNewComponent(Entity entity, T& component) {}
		virtual void handleDestroyedComponent(Entity entity) {}

	public:
		ComponentManager() {}

		void registerComponent()
		{
			m_componentArray = std::make_shared<ComponentArray<T>>();
		}

		void adoptComponentArray(const std::shared_ptr<ComponentManager<T>>& source)
		{
			m_componentArray = source->m_componentArray;
		}

		T& getNewComponent(Entity entity)
		{
			auto castedComponentArray = getComponentArray();
			T& component = castedComponentArray->getNewComponent(entity);

			handleNewComponent(entity, component);

			return component;
		}

		T& getComponent(Entity entity)
		{
			return getComponentArray()->getDataFromEntity(entity);
		}

		bool hasComponent(Entity entity) override
		{
			return getComponentArray()->hasData(entity);
		}

		void removeData(Entity entity) override
		{
			m_removedEntities.push(entity);
		}

		std::shared_ptr<ComponentArray<T>> getComponentArray()
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);
			return componentArray;
		}

		void freeRemovedComponents() override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);

			while (m_removedEntities.size() > 0)
			{
				Entity e = m_removedEntities.front();

				if (componentArray->hasData(e))
				{
					handleDestroyedComponent(e);
					componentArray->removeData(e);
				}

				m_removedEntities.pop();
			}
		}

		size_t getElementSize() const override
		{
			return sizeof(T);
		}

		bool isSnapshotSerializable() const override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);
			return is_snapshot_serializable_v<T> || !componentArray || componentArray->size == 0;
		}

		void clear() override
		{
			while (!m_removedEntities.empty())
			{
				m_removedEntities.pop();
			}
			auto componentArray = getComponentArray();
			if (componentArray)
			{
				componentArray->clear();
			}
		}

		std::shared_ptr<void> createEmptyArray() override
		{
			return std::make_shared<ComponentArray<T>>();
		}

		std::shared_ptr<void> getArray() const override
		{
			return m_componentArray;
		}

		void setArray(std::shared_ptr<void> arr) override
		{
			m_componentArray = std::move(arr);
			while (!m_removedEntities.empty())
				m_removedEntities.pop();
		}

		void serializeBinary(std::ostream& os) const override
		{
			if constexpr (is_snapshot_serializable_v<T>)
			{
				auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);
				uint32_t count = componentArray ? static_cast<uint32_t>(componentArray->size) : 0;
				uint32_t elemSize = static_cast<uint32_t>(sizeof(T));
				os.write(reinterpret_cast<const char*>(&count), sizeof(count));
				os.write(reinterpret_cast<const char*>(&elemSize), sizeof(elemSize));

				if (componentArray)
				{
					for (size_t i = 0; i < componentArray->size; ++i)
					{
						uint32_t entityId = componentArray->getEntityAtIdx(i).id;
						os.write(reinterpret_cast<const char*>(&entityId), sizeof(entityId));

						alignas(T) char cleanBuf[sizeof(T)];
						std::memset(cleanBuf, 0, sizeof(T));
						if constexpr (std::is_copy_assignable_v<T>)
						{
							*reinterpret_cast<T*>(cleanBuf) = componentArray->getDataAtIdx(i);
						}
						else
						{
							std::memcpy(cleanBuf, &componentArray->getDataAtIdx(i), sizeof(T));
						}
						os.write(cleanBuf, sizeof(T));
					}
				}
			}
			else
			{
				uint32_t count = 0;
				uint32_t elemSize = 0;
				os.write(reinterpret_cast<const char*>(&count), sizeof(count));
				os.write(reinterpret_cast<const char*>(&elemSize), sizeof(elemSize));
			}
		}

		bool deserializeToArray(std::istream& is, size_t fileElemSize, std::shared_ptr<void>& outArray) override
		{
			uint32_t count = 0;
			if (!readOrFail(is, &count, sizeof(count)))
				return false;
			uint32_t elemSize = 0;
			if (!readOrFail(is, &elemSize, sizeof(elemSize)))
				return false;

			if constexpr (is_snapshot_serializable_v<T>)
			{
				if (elemSize != sizeof(T))
				{
					Logger::error("[Snapshot] Component size mismatch! Expected " + std::to_string(sizeof(T)) +
								  ", got " + std::to_string(elemSize) + ".");
					return false;
				}

				if (count > MAX_ENTITIES.id)
				{
					Logger::error("[Snapshot] Component count exceeds MAX_ENTITIES: " + std::to_string(count));
					return false;
				}

				auto tempArray = std::make_shared<ComponentArray<T>>();

				for (uint32_t i = 0; i < count; ++i)
				{
					uint32_t entityId = 0;
					if (!readOrFail(is, &entityId, sizeof(entityId)))
						return false;
					if (entityId >= MAX_ENTITIES.id)
					{
						Logger::error("[Snapshot] Component entity ID out of bounds: " + std::to_string(entityId));
						return false;
					}

					alignas(T) char dataBuf[sizeof(T)];
					std::memset(dataBuf, 0, sizeof(T));
					if (!readOrFail(is, dataBuf, sizeof(T)))
						return false;

					T data{};
					if constexpr (std::is_copy_assignable_v<T>)
					{
						data = *reinterpret_cast<const T*>(dataBuf);
					}
					else
					{
						std::memcpy(&data, dataBuf, sizeof(T));
					}
					tempArray->insertData(Entity{entityId}, data);
				}
				outArray = tempArray;
				return true;
			}
			else
			{
				if (count > 0 && elemSize > 0)
				{
					uint64_t skipBytes = static_cast<uint64_t>(count) * (sizeof(uint32_t) + elemSize);
					is.seekg(static_cast<std::streamoff>(skipBytes), std::ios::cur);
					if (!is.good())
						return false;
				}
				return false;
			}
		}

		bool deserializeBinary(std::istream& is, size_t fileElemSize) override
		{
			std::shared_ptr<void> tempArray;
			if (!deserializeToArray(is, fileElemSize, tempArray))
				return false;
			setArray(std::move(tempArray));
			return true;
		}
	};
} // namespace WeirdEngine
