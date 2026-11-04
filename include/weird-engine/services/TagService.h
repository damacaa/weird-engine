#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "weird-engine/ecs/Registry.h"

namespace WeirdEngine
{
	class TagService
	{
	public:
		TagService(std::unordered_map<std::string, std::vector<Entity>>& tagToEntities,
				   std::unordered_map<Entity, std::vector<std::string>>& entityToTags)
			: m_tagToEntities(tagToEntities)
			, m_entityToTags(entityToTags)
		{
		}

		void addTag(Entity entity, const std::string& tag)
		{
			if (tag.empty() || entity == INVALID_ENTITY)
				return;

			auto& tags = m_entityToTags[entity];
			if (std::find(tags.begin(), tags.end(), tag) == tags.end())
			{
				tags.push_back(tag);
			}

			auto& entities = m_tagToEntities[tag];
			if (std::find(entities.begin(), entities.end(), entity) == entities.end())
			{
				entities.push_back(entity);
			}
		}

		void removeTag(Entity entity, const std::string& tag)
		{
			auto itEntity = m_entityToTags.find(entity);
			if (itEntity != m_entityToTags.end())
			{
				auto& tags = itEntity->second;
				tags.erase(std::remove(tags.begin(), tags.end(), tag), tags.end());
				if (tags.empty())
					m_entityToTags.erase(itEntity);
			}

			auto itTag = m_tagToEntities.find(tag);
			if (itTag != m_tagToEntities.end())
			{
				auto& entities = itTag->second;
				entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
				if (entities.empty())
					m_tagToEntities.erase(itTag);
			}
		}

		void removeAllTags(Entity entity)
		{
			auto itEntity = m_entityToTags.find(entity);
			if (itEntity == m_entityToTags.end())
				return;

			for (const auto& tag : itEntity->second)
			{
				auto itTag = m_tagToEntities.find(tag);
				if (itTag != m_tagToEntities.end())
				{
					auto& entities = itTag->second;
					entities.erase(std::remove(entities.begin(), entities.end(), entity), entities.end());
					if (entities.empty())
						m_tagToEntities.erase(itTag);
				}
			}

			m_entityToTags.erase(itEntity);
		}

		bool hasTag(Entity entity, const std::string& tag) const
		{
			auto it = m_entityToTags.find(entity);
			if (it == m_entityToTags.end())
				return false;
			const auto& tags = it->second;
			return std::find(tags.begin(), tags.end(), tag) != tags.end();
		}

		const std::vector<std::string>& getTags(Entity entity) const
		{
			static const std::vector<std::string> emptyTags;
			auto it = m_entityToTags.find(entity);
			return (it != m_entityToTags.end()) ? it->second : emptyTags;
		}

		const std::vector<Entity>& getEntitiesWithTag(const std::string& tag) const
		{
			static const std::vector<Entity> emptyEntities;
			auto it = m_tagToEntities.find(tag);
			return (it != m_tagToEntities.end()) ? it->second : emptyEntities;
		}

		// Ergonomic aliases
		void add(Entity entity, const std::string& tag)
		{
			addTag(entity, tag);
		}

		void remove(Entity entity, const std::string& tag)
		{
			removeTag(entity, tag);
		}

		bool has(Entity entity, const std::string& tag) const
		{
			return hasTag(entity, tag);
		}

		const std::vector<Entity>& getEntities(const std::string& tag) const
		{
			return getEntitiesWithTag(tag);
		}

	private:
		std::unordered_map<std::string, std::vector<Entity>>& m_tagToEntities;
		std::unordered_map<Entity, std::vector<std::string>>& m_entityToTags;
	};
} // namespace WeirdEngine
