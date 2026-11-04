#pragma once

#include "ComponentManager.h"
#include "Entity.h"
#include "SceneState.h"
#include "weird-engine/Assert.h"
#include "weird-engine/Logger.h"

#include <bitset>
#include <cstddef>
#include <iostream>
#include <istream>
#include <limits>
#include <memory>
#include <ostream>
#include <queue>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
#include <cstdlib>
#include <cxxabi.h>
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

		// Scene states use their own counter so the state store is sized by the
		// number of state types, independent of how many component types exist.
		inline size_t nextSceneStateTypeId()
		{
			static size_t id = 0;
			return id++;
		}

		template <typename T> size_t getSceneStateTypeId()
		{
			static const size_t id = nextSceneStateTypeId();
			return id;
		}
	} // namespace internal

	// Entity + component storage. Systems are not managed here: scheduling
	// lives on Scene, which dispatches them with a Registry& and
	// ServiceProvider&.
	class Registry
	{
	public:
		static constexpr uint32_t SNAPSHOT_MAGIC = 0x504E5357; // 'WSNP'
		static constexpr uint32_t SNAPSHOT_VERSION = 1;

		Entity createEntity()
		{
			if (!m_freeEntities.empty())
			{
				Entity e = m_freeEntities.front();
				m_freeEntities.pop();
				m_destroyedEntities.reset(e.id);
				return e;
			}
			if (m_entityCount >= MAX_ENTITIES)
			{
				if (m_droppedEntityCount == 0 || m_droppedEntityCount % 1000 == 0)
				{
					Logger::error("[Registry] Entity limit reached: maximum capacity of " +
								  std::to_string(MAX_ENTITIES.id) + " entities reached. Cannot create new entity (" +
								  std::to_string(m_droppedEntityCount + 1) + " dropped). Returning INVALID_ENTITY.");
				}
				++m_droppedEntityCount;
				return INVALID_ENTITY;
			}
			return m_entityCount++;
		}

		bool isEntityValid(Entity entity) const noexcept
		{
			return entity < MAX_ENTITIES;
		}

		bool isEntityAlive(Entity entity) const noexcept
		{
			return entity < m_entityCount && !m_destroyedEntities.test(entity.id);
		}

		bool isEntityDestroyed(Entity entity) const noexcept
		{
			return entity >= m_entityCount || m_destroyedEntities.test(entity.id);
		}

		bool canCreateEntity() const noexcept
		{
			return !m_freeEntities.empty() || m_entityCount < MAX_ENTITIES;
		}

		size_t getAvailableEntityCount() const noexcept
		{
			return m_freeEntities.size() + (m_entityCount < MAX_ENTITIES ? (MAX_ENTITIES.id - m_entityCount.id) : 0);
		}

		Entity getEntityCount() const
		{
			return m_entityCount;
		}

		void destroyEntity(Entity entity)
		{
			if (entity >= m_entityCount || entity >= MAX_ENTITIES || m_destroyedEntities.test(entity.id))
				return;
			m_destroyedEntities.set(entity.id);
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

			bool freedAny = !m_entitiesToFree.empty();
			while (!m_entitiesToFree.empty())
			{
				m_freeEntities.push(m_entitiesToFree.front());
				m_entitiesToFree.pop();
			}
			if (freedAny)
			{
				m_droppedEntityCount = 0;
			}
		}

		// Component accessors normalize T with std::remove_cvref_t, so
		// cv-qualified or reference spellings (e.g. getComponent<const
		// Transform>()) always resolve to the stored component type.
		template <typename T> std::remove_cvref_t<T>& addComponent(Entity entity)
		{
			using ComponentType = std::remove_cvref_t<T>;
			if (entity >= MAX_ENTITIES)
			{
				Logger::error("[Registry] Cannot add component: entity ID (" + std::to_string(entity.id) +
							  ") is invalid (maximum entity capacity is " + std::to_string(MAX_ENTITIES.id) + ").");
				static ComponentType dummy{};
				dummy = ComponentType{};
				return dummy;
			}
			auto cm = getComponentManager<ComponentType>();
			auto& component = cm->getNewComponent(entity);

			return component;
		}

		template <typename T> std::remove_reference_t<T>& getComponent(Entity entity)
		{
			using ComponentType = std::remove_cvref_t<T>;
			if (entity >= MAX_ENTITIES)
			{
				Logger::error("[Registry] Cannot get component from invalid entity (" + std::to_string(entity.id) +
							  "): entity ID exceeds or equals MAX_ENTITIES (" + std::to_string(MAX_ENTITIES.id) + ").");
				static ComponentType dummy{};
				dummy = ComponentType{};
				return dummy;
			}
			return getComponentManager<ComponentType>()->getComponent(entity);
		}

		template <typename T> Entity getComponentOwner(const T& component)
		{
			using ComponentType = std::remove_cvref_t<T>;
			return getComponentManager<ComponentType>()->getComponentArray()->getEntityFromComponent(component);
		}

		template <typename T> bool hasComponent(Entity entity) const
		{
			using ComponentType = std::remove_cvref_t<T>;
			if (entity >= MAX_ENTITIES)
				return false;
			size_t id = internal::getComponentTypeId<ComponentType>();
			if (id >= m_componentManagers.size() || !m_componentManagers[id])
				return false;
			return std::static_pointer_cast<ComponentManager<ComponentType>>(m_componentManagers[id])
				->hasComponent(entity);
		}

		template <typename T> void setEntityDirty(Entity entity, bool dirty = true)
		{
			using ComponentType = std::remove_cvref_t<T>;
			if (entity >= MAX_ENTITIES)
				return;
			getComponentManager<ComponentType>()->getComponentArray()->setEntityDirty(entity, dirty);
		}

		template <typename T> void setComponentDirty(const T& component, bool dirty = true)
		{
			using ComponentType = std::remove_cvref_t<T>;
			getComponentManager<ComponentType>()->getComponentArray()->setComponentDirty(component, dirty);
		}

		template <typename T> bool isComponentDirty(const T& component)
		{
			using ComponentType = std::remove_cvref_t<T>;
			return getComponentManager<ComponentType>()->getComponentArray()->isComponentDirty(component);
		}

		// =====================================================================
		// forEach — iterate entities that have ALL of the listed components
		// =====================================================================

		// Primary form: lambda receives (Entity e, T&...)
		template <typename... Ts, typename Func> void forEach(Func&& func)
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
						: (idx++, false)),
			 ...);

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

			using DriverType = std::remove_cvref_t<Driver>;
			auto driverArray = std::get<std::shared_ptr<ComponentArray<DriverType>>>(arrays);

			for (size_t i = 0; i < smallestSize; i++)
			{
				Entity e = driverArray->getEntityAtIdx(i);

				// Check that this entity exists in ALL component arrays
				bool hasAll =
					(std::get<std::shared_ptr<ComponentArray<std::remove_cvref_t<Ts>>>>(arrays)->hasData(e) && ...);
				if (!hasAll)
					continue;

				// Call the user's lambda with (Entity, T&...)
				func(e,
					 static_cast<std::remove_reference_t<Ts>&>(
						 std::get<std::shared_ptr<ComponentArray<std::remove_cvref_t<Ts>>>>(arrays)->getDataFromEntity(
							 e))...);
			}

			return true; // signal that we ran the loop
		}

	public:
		template <typename T> void registerComponent()
		{
			using ComponentType = std::remove_cvref_t<T>;
			size_t id = internal::getComponentTypeId<ComponentType>();

			if (id < m_componentManagers.size() && m_componentManagers[id])
				return;

			ComponentManager<ComponentType> manager;
			manager.registerComponent();
			auto pointerToManager = std::make_shared<ComponentManager<ComponentType>>(manager);

			if (id >= m_componentManagers.size())
				m_componentManagers.resize(id + 1);
			m_componentManagers[id] = pointerToManager;
			cacheComponentName<ComponentType>(id);
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

		template <typename T> std::shared_ptr<ComponentManager<std::remove_cvref_t<T>>> getComponentManager()
		{
			using ComponentType = std::remove_cvref_t<T>;

			size_t id = internal::getComponentTypeId<ComponentType>();

			if (id >= m_componentManagers.size() || !m_componentManagers[id])
			{
				registerComponent<ComponentType>();
			}

			return std::static_pointer_cast<ComponentManager<ComponentType>>(m_componentManagers[id]);
		}

		template <typename T> std::shared_ptr<ComponentArray<std::remove_cvref_t<T>>> getComponentArray()
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

		// =====================================================================
		// Scene state -- one instance per type, owned by the registry.
		//
		// Scene state is scene-scoped data shared by systems (level state,
		// caches, tables). Unlike components it is not attached to an entity,
		// is never copied, and supports snapshots for value-only types. Main thread only, like the
		// rest of the registry.
		//
		//   GolfingState& state = registry.emplaceState<GolfingState>();
		//   if (GolfingState* state = registry.getState<GolfingState>()) { ... }
		// =====================================================================

		// T is normalized with std::remove_cvref_t, so cv-qualified or
		// reference spellings (e.g. getState<const GolfingState>()) resolve to
		// the same state as the plain type.
		//
		// Creates (or replaces) the scene state of type T, built in place from
		// `args`. Returns a reference that stays valid until the state is
		// replaced or removed. Replacing an existing state invalidates all
		// references to the previous one.
		template <typename T, typename... Args> std::remove_cvref_t<T>& emplaceState(Args&&... args)
		{
			using StateType = std::remove_cvref_t<T>;
			auto holder = std::make_unique<SceneStateHolder<StateType>>(std::forward<Args>(args)...);
			StateType& value = holder->value;

			const size_t id = sceneStateIndex<StateType>();
			if (id >= m_sceneStates.size())
				m_sceneStates.resize(id + 1);
			m_sceneStates[id] = std::move(holder);
			cacheSceneStateName<StateType>(id);

			return value;
		}

		// Returns the scene state of type T, or nullptr when none was created.
		template <typename T> std::remove_reference_t<T>* getState()
		{
			using StateType = std::remove_cvref_t<T>;
			const size_t id = sceneStateIndex<StateType>();
			if (id >= m_sceneStates.size() || !m_sceneStates[id])
				return nullptr;
			return &static_cast<SceneStateHolder<StateType>*>(m_sceneStates[id].get())->value;
		}

		template <typename T> const std::remove_reference_t<T>* getState() const
		{
			using StateType = std::remove_cvref_t<T>;
			const size_t id = sceneStateIndex<StateType>();
			if (id >= m_sceneStates.size() || !m_sceneStates[id])
				return nullptr;
			return &static_cast<const SceneStateHolder<StateType>*>(m_sceneStates[id].get())->value;
		}

		template <typename T> bool hasState() const
		{
			const size_t id = sceneStateIndex<T>();
			return id < m_sceneStates.size() && m_sceneStates[id] != nullptr;
		}

		template <typename T> void removeState()
		{
			const size_t id = sceneStateIndex<T>();
			if (id < m_sceneStates.size())
				m_sceneStates[id].reset();
		}

		std::string getSceneStateName(size_t typeId) const
		{
			auto it = m_sceneStateNames.find(typeId);
			if (it != m_sceneStateNames.end())
				return it->second;
			return "Unknown";
		}

		// =====================================================================
		// Binary Snapshot System
		// =====================================================================

		bool canSerializeSnapshot() const
		{
			for (const auto& manager : m_componentManagers)
			{
				if (manager && !manager->isSnapshotSerializable())
					return false;
			}
			for (const auto& state : m_sceneStates)
			{
				if (state && !state->isSnapshotSerializable())
					return false;
			}
			return true;
		}

		bool serializeSnapshot(std::ostream& os, double simulationTime = 0.0)
		{
			if (!canSerializeSnapshot())
			{
				Logger::error("[Registry] Snapshot contains unsupported component or scene state types.");
				return false;
			}
			// Finish deferred removals so deleted entities and components cannot return after loading.
			freeRemovedComponents();

			os.write(reinterpret_cast<const char*>(&SNAPSHOT_MAGIC), sizeof(SNAPSHOT_MAGIC));
			os.write(reinterpret_cast<const char*>(&SNAPSHOT_VERSION), sizeof(SNAPSHOT_VERSION));
			os.write(reinterpret_cast<const char*>(&simulationTime), sizeof(simulationTime));

			// Entity counts
			uint32_t entityCount = m_entityCount.id;
			os.write(reinterpret_cast<const char*>(&entityCount), sizeof(entityCount));

			// Free entities
			std::queue<Entity> freeCopy = m_freeEntities;
			uint32_t freeCount = static_cast<uint32_t>(freeCopy.size());
			os.write(reinterpret_cast<const char*>(&freeCount), sizeof(freeCount));
			while (!freeCopy.empty())
			{
				uint32_t fe = freeCopy.front().id;
				os.write(reinterpret_cast<const char*>(&fe), sizeof(fe));
				freeCopy.pop();
			}

			// Component managers
			uint32_t validManagerCount = 0;
			for (size_t i = 0; i < m_componentManagers.size(); ++i)
			{
				if (m_componentManagers[i])
					++validManagerCount;
			}
			os.write(reinterpret_cast<const char*>(&validManagerCount), sizeof(validManagerCount));

			for (size_t i = 0; i < m_componentManagers.size(); ++i)
			{
				if (!m_componentManagers[i])
					continue;
				const std::string& name = m_componentSnapshotNames.at(i);
				uint32_t nameLen = static_cast<uint32_t>(name.size());
				os.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
				os.write(name.data(), nameLen);

				m_componentManagers[i]->serializeBinary(os);
			}

			// Scene states
			uint32_t validStateCount = 0;
			for (size_t i = 0; i < m_sceneStates.size(); ++i)
			{
				if (m_sceneStates[i])
					++validStateCount;
			}
			os.write(reinterpret_cast<const char*>(&validStateCount), sizeof(validStateCount));

			for (size_t i = 0; i < m_sceneStates.size(); ++i)
			{
				if (!m_sceneStates[i])
					continue;
				const std::string& name = m_sceneStateSnapshotNames.at(i);
				uint32_t nameLen = static_cast<uint32_t>(name.size());
				os.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
				os.write(name.data(), nameLen);

				m_sceneStates[i]->serializeBinary(os);
			}
			return static_cast<bool>(os);
		}

		bool deserializeSnapshot(std::istream& is, double* outSimulationTime = nullptr)
		{
			if (!canSerializeSnapshot())
			{
				Logger::error("[Registry] Cannot restore unsupported component or scene state types.");
				return false;
			}
			uint32_t magic = 0;
			is.read(reinterpret_cast<char*>(&magic), sizeof(magic));
			if (magic != SNAPSHOT_MAGIC)
			{
				Logger::error("[Registry] Invalid snapshot magic!");
				return false;
			}

			uint32_t version = 0;
			is.read(reinterpret_cast<char*>(&version), sizeof(version));
			if (version != SNAPSHOT_VERSION)
			{
				Logger::error("[Registry] Unsupported snapshot version: " + std::to_string(version));
				return false;
			}

			double simTime = 0.0;
			is.read(reinterpret_cast<char*>(&simTime), sizeof(simTime));
			if (outSimulationTime)
			{
				*outSimulationTime = simTime;
			}

			// Entity counts
			uint32_t entityCount = 0;
			is.read(reinterpret_cast<char*>(&entityCount), sizeof(entityCount));
			m_entityCount = Entity{entityCount};

			// Free entities
			while (!m_freeEntities.empty())
				m_freeEntities.pop();
			while (!m_entitiesToFree.empty())
				m_entitiesToFree.pop();
			m_droppedEntityCount = 0;
			m_destroyedEntities.reset();

			uint32_t freeCount = 0;
			is.read(reinterpret_cast<char*>(&freeCount), sizeof(freeCount));
			for (uint32_t i = 0; i < freeCount; ++i)
			{
				uint32_t fe = 0;
				is.read(reinterpret_cast<char*>(&fe), sizeof(fe));
				m_freeEntities.push(Entity{fe});
				m_destroyedEntities.set(fe);
			}

			// Clear all existing component managers before loading
			for (auto& mgr : m_componentManagers)
			{
				if (mgr)
					mgr->clear();
			}

			// Read components
			uint32_t managerCount = 0;
			is.read(reinterpret_cast<char*>(&managerCount), sizeof(managerCount));
			for (uint32_t i = 0; i < managerCount; ++i)
			{
				uint32_t nameLen = 0;
				is.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
				std::string name(nameLen, '\0');
				is.read(name.data(), nameLen);

				IComponentManager* targetMgr = nullptr;
				for (size_t m = 0; m < m_componentManagers.size(); ++m)
				{
					if (m_componentManagers[m] && m_componentSnapshotNames.at(m) == name)
					{
						targetMgr = m_componentManagers[m].get();
						break;
					}
				}

				if (targetMgr)
				{
					targetMgr->deserializeBinary(is, targetMgr->getElementSize());
				}
				else
				{
					Logger::warning("[Registry] Component '" + name + "' not registered in scene. Skipping its data.");
					uint32_t count = 0;
					is.read(reinterpret_cast<char*>(&count), sizeof(count));
					uint32_t elemSize = 0;
					is.read(reinterpret_cast<char*>(&elemSize), sizeof(elemSize));
					if (count > 0 && elemSize > 0)
					{
						is.seekg(count * (sizeof(uint32_t) + elemSize), std::ios::cur);
					}
				}
			}

			// Read scene states
			uint32_t stateCount = 0;
			is.read(reinterpret_cast<char*>(&stateCount), sizeof(stateCount));
			for (uint32_t i = 0; i < stateCount; ++i)
			{
				uint32_t nameLen = 0;
				is.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
				std::string name(nameLen, '\0');
				is.read(name.data(), nameLen);

				ISceneState* targetState = nullptr;
				for (size_t s = 0; s < m_sceneStates.size(); ++s)
				{
					if (m_sceneStates[s] && m_sceneStateSnapshotNames.at(s) == name)
					{
						targetState = m_sceneStates[s].get();
						break;
					}
				}

				if (targetState)
				{
					targetState->deserializeBinary(is, targetState->getStateSize());
				}
				else
				{
					Logger::warning("[Registry] SceneState '" + name + "' not registered in scene. Skipping its data.");
					uint32_t elemSize = 0;
					is.read(reinterpret_cast<char*>(&elemSize), sizeof(elemSize));
					if (elemSize > 0)
					{
						is.seekg(elemSize, std::ios::cur);
					}
				}
			}

			return true;
		}

	private:
		// Normalizes T and maps it to a slot in m_sceneStates.
		template <typename T> static size_t sceneStateIndex()
		{
			return internal::getSceneStateTypeId<std::remove_cvref_t<T>>();
		}

		template <typename T> static std::string demangleTypeName()
		{
			const char* rawName = typeid(T).name();
			std::string finalName;
#if (defined(__GNUC__) || defined(__clang__)) && !defined(_MSC_VER)
			int status = -4;
			char* demangled = abi::__cxa_demangle(rawName, nullptr, nullptr, &status);
			if (status == 0 && demangled)
			{
				finalName = demangled;
				std::free(demangled);
			}
			else
			{
				finalName = rawName;
				if (demangled)
					std::free(demangled);
			}
#else
			finalName = rawName;
#endif

			if (finalName.starts_with("struct "))
				finalName.erase(0, 7);
			else if (finalName.starts_with("class "))
				finalName.erase(0, 6);

			return finalName;
		}

		template <typename T> void cacheComponentName(size_t id)
		{
			if (m_componentNames.find(id) != m_componentNames.end())
				return;
			const std::string fullName = demangleTypeName<T>();
			m_componentSnapshotNames[id] = fullName;
			const size_t colonPos = fullName.rfind("::");
			m_componentNames[id] = colonPos == std::string::npos ? fullName : fullName.substr(colonPos + 2);
		}

		template <typename T> void cacheSceneStateName(size_t id)
		{
			if (m_sceneStateNames.find(id) != m_sceneStateNames.end())
				return;
			const std::string fullName = demangleTypeName<T>();
			m_sceneStateSnapshotNames[id] = fullName;
			const size_t colonPos = fullName.rfind("::");
			m_sceneStateNames[id] = colonPos == std::string::npos ? fullName : fullName.substr(colonPos + 2);
		}

		std::unordered_map<size_t, std::string> m_componentNames;
		std::unordered_map<size_t, std::string> m_sceneStateNames;
		std::unordered_map<size_t, std::string> m_componentSnapshotNames;
		std::unordered_map<size_t, std::string> m_sceneStateSnapshotNames;
		std::vector<std::unique_ptr<ISceneState>> m_sceneStates;
		std::vector<std::shared_ptr<IComponentManager>> m_componentManagers;
		std::queue<Entity> m_freeEntities;
		std::queue<Entity> m_entitiesToFree;
		std::bitset<MAX_ENTITIES.id> m_destroyedEntities;
		Entity m_entityCount = 0;
		size_t m_droppedEntityCount = 0;
	};
} // namespace WeirdEngine
