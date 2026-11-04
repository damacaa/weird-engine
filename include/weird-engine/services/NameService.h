#pragma once

#include <string>
#include <unordered_map>

#include "weird-engine/ecs/Registry.h"

namespace WeirdEngine
{
	/// Map from entity name (std::string) to the entity that owns it.
	using NameMap = std::unordered_map<std::string, Entity>;

	class NameService
	{
	public:
		NameService(NameMap& nameToEntity, std::unordered_map<Entity, std::string>& entityToName)
			: m_nameToEntity(nameToEntity)
			, m_entityToName(entityToName)
		{
		}

		// Assign a unique name to an entity. If the name is already owned by
		// another entity, it is moved to this one. An empty name is treated
		// as a removal request (equivalent to calling removeName).
		void setName(Entity entity, const std::string& name)
		{
			if (name.empty())
			{
				removeName(entity);
				return;
			}

			// If the name is already owned by another entity, remove it from that entity
			auto existingOwner = m_nameToEntity.find(name);
			if (existingOwner != m_nameToEntity.end() && existingOwner->second != entity)
			{
				m_entityToName.erase(existingOwner->second);
			}

			// Remove any previous name this entity had
			auto existingName = m_entityToName.find(entity);
			if (existingName != m_entityToName.end() && existingName->second != name)
			{
				m_nameToEntity.erase(existingName->second);
			}

			m_nameToEntity[name] = entity;
			m_entityToName[entity] = name;
		}

		void name(Entity entity, const std::string& name)
		{
			setName(entity, name);
		}

		void removeName(Entity entity)
		{
			auto it = m_entityToName.find(entity);
			if (it == m_entityToName.end())
				return;
			m_nameToEntity.erase(it->second);
			m_entityToName.erase(it);
		}

		std::string getEntityName(Entity entity) const
		{
			auto it = m_entityToName.find(entity);
			if (it == m_entityToName.end())
				return "";
			return it->second;
		}

		std::string getName(Entity entity) const
		{
			return getEntityName(entity);
		}

		Entity getEntityByName(const std::string& name) const
		{
			auto it = m_nameToEntity.find(name);
			if (it == m_nameToEntity.end())
				return MAX_ENTITIES;
			return it->second;
		}

		bool hasName(Entity entity) const
		{
			return m_entityToName.find(entity) != m_entityToName.end();
		}

	private:
		NameMap& m_nameToEntity;
		std::unordered_map<Entity, std::string>& m_entityToName;
	};
} // namespace WeirdEngine
