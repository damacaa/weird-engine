#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/NameService.h"
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	class Scene;

	struct ClusterInstance
	{
		uint32_t id = 0;
		std::string name;			   // Unique instance name (e.g. "boss", "guard_left")
		std::string type;			   // Cluster type (e.g. "man", derived from asset name)
		std::string assetPath;		   // e.g. "clusters/man.weird"
		vec2 position{0.0f, 0.0f};	   // Spawn or reference position
		float rotation{0.0f};		   // Spawn or reference rotation
		std::vector<Entity> entities;  // Member entities
		NameMap localNames;			   // Local names: "head" -> Entity, "hand" -> Entity
		std::vector<std::string> tags; // Instance tags (e.g. "enemy", "boss")

		Entity getEntity(const std::string& localName) const
		{
			auto it = localNames.find(localName);
			return (it != localNames.end()) ? it->second : INVALID_ENTITY;
		}

		Entity get(const std::string& localName) const
		{
			return getEntity(localName);
		}

		bool hasEntity(Entity entity) const
		{
			return std::find(entities.begin(), entities.end(), entity) != entities.end();
		}

		bool hasTag(const std::string& tag) const
		{
			return std::find(tags.begin(), tags.end(), tag) != tags.end();
		}
	};

	class ClusterService
	{
	public:
		ClusterService(Scene& scene, std::vector<ClusterInstance>& clusters);

		// Instantiates a cluster into the scene from a .weird file.
		// Automatically:
		// 1. Loads entities & constraints from assetPath into the scene.
		// 2. Extracts member local names (without polluting global NameService).
		// 3. Offsets and rotates member transforms and rigidbodies to (position, rotation).
		// 4. Applies instanceTags to all member entities in TagService and onto the ClusterInstance.
		// 5. Registers and returns the ClusterInstance.
		ClusterInstance& instantiate(const std::string& assetPath, vec2 position = vec2(0.0f), float rotation = 0.0f,
									 const std::string& instanceName = "",
									 const std::vector<std::string>& instanceTags = {});

		ClusterInstance* getById(uint32_t id);
		const ClusterInstance* getById(uint32_t id) const;

		ClusterInstance* getByName(const std::string& name);
		const ClusterInstance* getByName(const std::string& name) const;

		std::vector<ClusterInstance*> getByType(const std::string& type);
		std::vector<const ClusterInstance*> getByType(const std::string& type) const;

		std::vector<ClusterInstance*> getByTag(const std::string& tag);
		std::vector<const ClusterInstance*> getByTag(const std::string& tag) const;

		// Reverse lookup: which cluster owns this entity?
		ClusterInstance* getClusterForEntity(Entity entity);
		const ClusterInstance* getClusterForEntity(Entity entity) const;

		const std::vector<ClusterInstance>& getAll() const
		{
			return m_clusters;
		}
		std::vector<ClusterInstance>& getAll()
		{
			return m_clusters;
		}

		// Destruction
		void destroyCluster(uint32_t id);
		void destroyCluster(ClusterInstance* cluster);
		void destroyAll();

		// Internal registration used by SceneSerializer
		ClusterInstance& addInstance(ClusterInstance instance);

	private:
		Scene& m_scene;
		std::vector<ClusterInstance>& m_clusters;
		uint32_t m_nextClusterId = 1;
	};
} // namespace WeirdEngine
