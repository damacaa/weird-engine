#pragma once

#include <string>
#include <unordered_set>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/NameService.h"

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
		// If restoreCamera is true, camera state in the file overrides the main camera.
		// Returns a map of entity names to their corresponding entities.
		NameMap loadWeirdFile(const std::string& path, bool blacklistEntities = false, bool restoreCamera = false);

		// Save full registry binary snapshot
		void saveSnapshot(const std::string& filename);

		// Restore full registry binary snapshot
		bool loadSnapshot(const std::string& filename);

		// Delete the scene's configured .weirdsnap file
		void deleteSnapshotFile();

		// Set the path to a .weirdsnap file to load when the scene starts
		void setSnapshotFilePath(const std::string& path);

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
