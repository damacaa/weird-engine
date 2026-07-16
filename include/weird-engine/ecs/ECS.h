#pragma once

#include "ComponentManager.h"
#include "Entity.h"

#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <tuple>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <string>
#include <typeinfo>
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace WeirdEngine
{
	namespace internal
	{
		inline size_t nextComponentTypeId()
		{
			static size_t id = 0;
			return id++;
		}

		template <typename T> size_t getComponentTypeId()
		{
			// Each component type gets a unique ID at compile time
			static const size_t id = nextComponentTypeId();
			return id;
		}
	} // namespace internal

	// ECSManager Manager
	class ECSManager
	{
	public:
		Entity createEntity()
		{
			if (!m_freeEntities.empty())
			{
				Entity e = m_freeEntities.front();
				m_freeEntities.pop();
				return e;
			}
			return m_entityCount++;
		}

		Entity getEntityCount() const
		{
			return m_entityCount;
		}

		void destroyEntity(Entity entity)
		{
			for (auto const& manager : m_componentManagers)
			{
				if (manager)
					manager->removeData(entity);
			}
			m_entitiesToFree.push(entity);
		}

		void freeRemovedComponents()
		{
			for (auto const& manager : m_componentManagers)
			{
				if (manager)
					manager->freeRemovedComponents();
			}

			while (!m_entitiesToFree.empty())
			{
				m_freeEntities.push(m_entitiesToFree.front());
				m_entitiesToFree.pop();
			}
		}

		template <typename T> T& addComponent(Entity entity)
		{
			auto cm = getComponentManager<T>();
			auto& component = cm->getNewComponent(entity);

			return component;
		}

		template <typename T> T& getComponent(Entity entity)
		{
			return getComponentManager<T>()->getComponent(entity);
		}

		template <typename T> bool hasComponent(Entity entity) const
		{
			size_t id = internal::getComponentTypeId<T>();
			if (id >= m_componentManagers.size() || !m_componentManagers[id])
				return false;
			return std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id])->hasComponent(entity);
		}

		template <typename T> void setEntityDirty(Entity entity, bool dirty = true)
		{
			getComponentManager<T>()->getComponentArray()->setEntityDirty(entity, dirty);
		}

		template <typename T> void setComponentDirty(const T& component, bool dirty = true)
		{
			getComponentManager<T>()->getComponentArray()->setComponentDirty(component, dirty);
		}

		template <typename T> bool isComponentDirty(const T& component)
		{
			return getComponentManager<T>()->getComponentArray()->isComponentDirty(component);
		}

		// =====================================================================
		// forEach — iterate entities that have ALL of the listed components
		// =====================================================================

		// Primary form: lambda receives (Entity e, T&...)
		template <typename... Ts, typename Func>
		void forEach(Func&& func)
		{
			// Grab all the component arrays up front
			auto arrays = std::make_tuple(getComponentArray<Ts>()...);

			// Find the smallest array to drive the outer loop.
			// We iterate the smallest one and check membership in the rest.
			size_t smallestSize = std::numeric_limits<size_t>::max();
			size_t smallestIdx = 0;
			size_t idx = 0;
			((void)(getComponentArray<Ts>()->getSize() < smallestSize
				? (smallestSize = getComponentArray<Ts>()->getSize(), smallestIdx = idx++, true)
				: (idx++, false)), ...);

			// Dispatch to the driver that iterates the smallest array
			idx = 0;
			(forEachDriver<Ts, Ts...>(std::forward<Func>(func), arrays, smallestSize, smallestIdx, idx++) || ...);
		}

	private:

		// Driver: called once per type in the pack. Only the one matching
		// smallestIdx actually runs the loop; the others return false.
		template <typename Driver, typename... Ts, typename Func, typename Arrays>
		bool forEachDriver(Func&& func, Arrays& arrays, size_t smallestSize, size_t targetIdx, size_t myIdx)
		{
			if (myIdx != targetIdx)
				return false;

			auto driverArray = std::get<std::shared_ptr<ComponentArray<Driver>>>(arrays);

			for (size_t i = 0; i < smallestSize; i++)
			{
				Entity e = driverArray->getEntityAtIdx(i);

				// Check that this entity exists in ALL component arrays
				bool hasAll = (std::get<std::shared_ptr<ComponentArray<Ts>>>(arrays)->hasData(e) && ...);
				if (!hasAll)
					continue;

				// Call the user's lambda with (Entity, T&...)
				func(e, std::get<std::shared_ptr<ComponentArray<Ts>>>(arrays)->getDataFromEntity(e)...);
			}

			return true; // signal that we ran the loop
		}

	public:

		template <typename T> void registerComponent()
		{

			ComponentManager<T> manager;
			manager.registerComponent();
			auto pointerToManager = std::make_shared<ComponentManager<T>>(manager);

			size_t id = internal::getComponentTypeId<T>();
			if (id >= m_componentManagers.size())
				m_componentManagers.resize(id + 1);
			m_componentManagers[id] = pointerToManager;
			cacheComponentName<T>(id);
		}

		template <typename T> void registerComponent(std::shared_ptr<ComponentManager<T>> manager)
		{
			size_t id = internal::getComponentTypeId<T>();
			if (id < m_componentManagers.size() && m_componentManagers[id])
			{
				// A default manager already exists (e.g. cached by a system).
				// Adopt its ComponentArray so both the cached and new manager
				// share the same data.
				auto existing = std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id]);
				manager->adoptComponentArray(existing);
			}
			else
			{
				manager->registerComponent();
			}
			if (id >= m_componentManagers.size())
				m_componentManagers.resize(id + 1);
			m_componentManagers[id] = manager;
			cacheComponentName<T>(id);
		}

		template <typename T> std::shared_ptr<ComponentManager<T>> getComponentManager()
		{

			size_t id = internal::getComponentTypeId<T>();

			if (id >= m_componentManagers.size() || !m_componentManagers[id])
			{
				registerComponent<T>();
			}

			return std::static_pointer_cast<ComponentManager<T>>(m_componentManagers[id]);
		}

		template <typename T> std::shared_ptr<ComponentArray<T>> getComponentArray()
		{
			return getComponentManager<T>()->getComponentArray();
		}

		std::vector<size_t> getComponentTypes(Entity entity) const
		{
			std::vector<size_t> componentTypes;
			for (size_t i = 0; i < m_componentManagers.size(); ++i)
			{
				if (m_componentManagers[i] && m_componentManagers[i]->hasComponent(entity))
				{
					componentTypes.push_back(i);
				}
			}
			return componentTypes;
		}

		std::string getComponentName(size_t typeId) const
		{
			auto it = m_componentNames.find(typeId);
			if (it != m_componentNames.end())
				return it->second;
			return "Unknown";
		}

	private:
		template <typename T> void cacheComponentName(size_t id)
		{
			if (m_componentNames.find(id) != m_componentNames.end()) return;

			const char* rawName = typeid(T).name();
			std::string finalName;
#if defined(__GNUC__) || defined(__clang__)
			int status = -4;
			char* demangled = abi::__cxa_demangle(rawName, nullptr, nullptr, &status);
			if (status == 0 && demangled) {
				finalName = demangled;
				std::free(demangled);
			} else {
				finalName = rawName;
				if (demangled) std::free(demangled);
			}
#else
			finalName = rawName;
#endif

			// Strip namespaces (e.g. "WeirdEngine::Transform" -> "Transform")
			size_t colonPos = finalName.rfind("::");
			if (colonPos != std::string::npos) {
				finalName = finalName.substr(colonPos + 2);
			} else {
				// Strip "class " or "struct " prefixes if there was no namespace (mostly for MSVC)
				size_t spacePos = finalName.rfind(" ");
				if (spacePos != std::string::npos) {
					finalName = finalName.substr(spacePos + 1);
				}
			}

			m_componentNames[id] = finalName;
		}

		std::unordered_map<size_t, std::string> m_componentNames;
		std::vector<std::shared_ptr<IComponentManager>> m_componentManagers;
		std::queue<Entity> m_freeEntities;
		std::queue<Entity> m_entitiesToFree;
		Entity m_entityCount = 0;
	};
} // namespace WeirdEngine

#include "weird-engine/components/FlyMovement.h"
#include "weird-engine/components/FlyMovement2D.h"
#include "weird-engine/components/Transform.h"
#include "weird-physics/components/RigidBody.h"
#include "weird-renderer/components/Button.h"
#include "weird-renderer/components/Camera.h"
#include "weird-renderer/components/CustomShape.h"
#include "weird-renderer/components/InstancedMeshRenderer.h"
#include "weird-renderer/components/MeshRenderer.h"
#include "weird-renderer/components/SDFRenderer.h"
#include "weird-renderer/components/TextRenderer.h"
