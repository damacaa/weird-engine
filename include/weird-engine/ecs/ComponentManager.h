#pragma once

#include <queue>

#include "ComponentArray.h"
#include "Entity.h"
#include "Component.h"



namespace WeirdEngine
{

	class ComponentManager 
	{

	protected:

		std::shared_ptr<void> m_componentArray;
		std::queue<Entity> m_removedEntities;

		virtual void HandleDestroyedComponent(Entity entity) {}

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
			return getComponentArray<T>()->getDataFromEntity(entity);
		}

		template <typename T>
		bool hasComponent(Entity entity) {
			return getComponentArray<T>()->hasData(entity);
		}

		

		void removeData(Entity entity) 
		{
			m_removedEntities.push(entity);
		}


		template <typename T>
		std::shared_ptr<ComponentArray<T>> getComponentArray() {

			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);

			freeRemovedComponents<T>();

			return componentArray;

		}


		template <typename T>
		void freeRemovedComponents() 
		{

			auto componentArray = std::static_pointer_cast<ComponentArray<T>>(m_componentArray);

			while (m_removedEntities.size() > 0)
			{
				Entity e = m_removedEntities.front();

				if (componentArray->hasData(e))
				{
					HandleDestroyedComponent(e);
					componentArray->removeData(e);
				}

				m_removedEntities.pop();
			}
		}

	};
}