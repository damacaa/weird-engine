#pragma once

#include <string>
#include <unordered_map>

#include "weird-engine/ecs/Registry.h"

namespace WeirdEngine
{
	/// Map from tag name (std::string) to the entity that owns it.
	using TagMap = std::unordered_map<std::string, Entity>;

	class TagService
	{
	public:
		TagService(TagMap& tagToEntity, std::unordered_map<Entity, std::string>& entityToTag)
			: m_tagToEntity(tagToEntity)
			, m_entityToTag(entityToTag)
		{
		}

		// Assign a unique tag to an entity. If the tag is already owned by
		// another entity, it is moved to this one. An empty name is treated
		// as a removal request (equivalent to calling removeTag).
		void tag(Entity entity, const std::string& name)
		{
			if (name.empty())
			{
				removeTag(entity);
				return;
			}

			// If the tag is already owned by another entity, remove it from that entity
			auto existingOwner = m_tagToEntity.find(name);
			if (existingOwner != m_tagToEntity.end() && existingOwner->second != entity)
			{
				m_entityToTag.erase(existingOwner->second);
			}

			// Remove any previous tag this entity had
			auto existingTag = m_entityToTag.find(entity);
			if (existingTag != m_entityToTag.end() && existingTag->second != name)
			{
				m_tagToEntity.erase(existingTag->second);
			}

			m_tagToEntity[name] = entity;
			m_entityToTag[entity] = name;
		}

		void removeTag(Entity entity)
		{
			auto it = m_entityToTag.find(entity);
			if (it == m_entityToTag.end())
				return;
			m_tagToEntity.erase(it->second);
			m_entityToTag.erase(it);
		}

		std::string getEntityTag(Entity entity) const
		{
			auto it = m_entityToTag.find(entity);
			if (it == m_entityToTag.end())
				return "";
			return it->second;
		}

		Entity getEntityByTag(const std::string& name) const
		{
			auto it = m_tagToEntity.find(name);
			if (it == m_tagToEntity.end())
				return MAX_ENTITIES;
			return it->second;
		}

	private:
		TagMap& m_tagToEntity;
		std::unordered_map<Entity, std::string>& m_entityToTag;
	};
} // namespace WeirdEngine
