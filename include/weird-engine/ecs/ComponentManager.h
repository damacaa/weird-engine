#pragma once

#include <memory>
#include <queue>

#include "Component.h"
#include "ComponentArray.h"
#include "Entity.h"

namespace WeirdEngine
{
	class IComponentManager
	{
	public:
		virtual ~IComponentManager() = default;
		virtual void freeRemovedComponents() = 0;
		virtual void removeData(Entity entity) = 0;
		virtual bool hasComponent(Entity entity) = 0;
	};

	template <typename T> class ComponentManager : public IComponentManager
	{
	protected:
		std::shared_ptr<void> m_componentArray;
		std::vector<Entity> m_removedEntities;

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

			component.Owner = entity;

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
			m_removedEntities.push_back(entity);
		}

		std::shared_ptr<ComponentArray<T>> getComponentArray()
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);
			return componentArray;
		}

		void freeRemovedComponents() override
		{
			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);

			for (Entity e : m_removedEntities)
			{
				if (componentArray->hasData(e))
				{
					handleDestroyedComponent(e);
					componentArray->removeData(e);
				}
			}
			m_removedEntities.clear();
		}
	};
} // namespace WeirdEngine