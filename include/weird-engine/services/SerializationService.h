#pragma once

#include <string>
#include <unordered_set>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/TagService.h"

namespace WeirdEngine
{
	class Scene;

	class SerializationService
	{
	public:
		SerializationService(Scene& scene, std::unordered_set<Entity>& blacklist, std::string& sceneFilePath)
			: m_scene(scene)
			, m_blacklist(blacklist)
			, m_sceneFilePath(sceneFilePath)
		{
		}

		// Save the current scene state to a .weird JSON file
		void saveScene(const std::string& filename);

		void deleteSceneFile();

		// Dynamically load a .weird file and add its contents to the scene.
		// If blacklistEntities is true, all entities created by the load will be
		// excluded from future scene serialization.
		// Returns a map of tag names to their corresponding entities.
		TagMap loadWeirdFile(const std::string& path, bool blacklistEntities = false);

		void blacklistEntity(Entity entity)
		{
			m_blacklist.insert(entity);
		}

		void unblacklistEntity(Entity entity)
		{
			m_blacklist.erase(entity);
		}

		bool isBlacklisted(Entity entity) const
		{
			return m_blacklist.find(entity) != m_blacklist.end();
		}

		// Set the path to a .weird file to load when the scene starts
		void setSceneFilePath(const std::string& path)
		{
			m_sceneFilePath = path;
		}

	private:
		Scene& m_scene;
		std::unordered_set<Entity>& m_blacklist;
		std::string& m_sceneFilePath;
	};
} // namespace WeirdEngine
