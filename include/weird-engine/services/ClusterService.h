#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/NameService.h"
#include "weird-engine/vec.h"

namespace WeirdEngine
{
	class Scene;

	namespace ClusterOverrideFlag
	{
		constexpr uint32_t None = 0;
		constexpr uint32_t BodyType = 1u << 0;
		constexpr uint32_t Mass = 1u << 1;
		constexpr uint32_t EnableCollision = 1u << 2;
		constexpr uint32_t EnableShapeCollision = 1u << 3;
		constexpr uint32_t DotIsStatic = 1u << 4;
		constexpr uint32_t DotMaterial = 1u << 5;
		constexpr uint32_t ShapeMaterial = 1u << 6;
		constexpr uint32_t ShapeHasCollisions = 1u << 7;
		constexpr uint32_t ShapeCombination = 1u << 8;
		constexpr uint32_t ShapeSmoothFactor = 1u << 9;
		constexpr uint32_t ShapeLayer = 1u << 10;
		constexpr uint32_t ShapeParameters = 1u << 11;
		constexpr uint32_t Position = 1u << 12;
		constexpr uint32_t Rotation = 1u << 13;
		constexpr uint32_t Scale = 1u << 14;
		constexpr uint32_t GlobalName = 1u << 15;
		constexpr uint32_t CustomTags = 1u << 16;
		constexpr uint32_t TextMaterial = 1u << 17;
		constexpr uint32_t TextContent = 1u << 18;
		constexpr uint32_t TextSize = 1u << 19;
		constexpr uint32_t ShapeOrder = 1u << 20;
	} // namespace ClusterOverrideFlag

	struct ClusterInstance
	{
		uint32_t id = 0;
		std::string name;									  // Unique instance name (e.g. "boss", "guard_left")
		std::string type;									  // Cluster type (e.g. "man", derived from asset name)
		std::string assetPath;								  // e.g. "clusters/man.weirdcluster"
		vec2 position{0.0f, 0.0f};							  // Spawn or reference position
		float rotation{0.0f};								  // Spawn or reference rotation
		std::vector<Entity> entities;						  // Member entities
		NameMap localNames;									  // Local names: "head" -> Entity, "hand" -> Entity
		std::vector<std::string> tags;						  // Instance tags (e.g. "enemy", "boss")
		std::unordered_map<Entity, Entity> sourceEntityIds;	  // Runtime entity -> template entity ID in .weirdcluster
		std::unordered_map<Entity, uint32_t> entityOverrides; // Runtime entity -> ClusterOverrideFlag bitmask
		bool hasOverrideMetadata = false;

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

		void markOverride(Entity entity, uint32_t flags)
		{
			if (entity != INVALID_ENTITY && flags != ClusterOverrideFlag::None)
			{
				entityOverrides[entity] |= flags;
			}
		}

		void clearOverride(Entity entity, uint32_t flags)
		{
			auto it = entityOverrides.find(entity);
			if (it == entityOverrides.end())
			{
				return;
			}
			it->second &= ~flags;
			if (it->second == ClusterOverrideFlag::None)
			{
				entityOverrides.erase(it);
			}
		}

		uint32_t getOverrides(Entity entity) const
		{
			auto it = entityOverrides.find(entity);
			return (it != entityOverrides.end()) ? it->second : ClusterOverrideFlag::None;
		}

		Entity getSourceEntityId(Entity entity) const
		{
			auto it = sourceEntityIds.find(entity);
			return (it != sourceEntityIds.end()) ? it->second : INVALID_ENTITY;
		}
	};

	class ClusterService
	{
	public:
		ClusterService(Scene& scene, std::vector<ClusterInstance>& clusters);

		// Instantiates a cluster into the scene from a .weirdcluster file.
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
		void clear();
		void syncNextClusterId();

		// Reloading / Updating
		bool reloadInstance(uint32_t id, const std::string& overrideAssetPath = "",
							std::unordered_map<Entity, Entity>* outOldToNewEntities = nullptr);
		void markEntityOverride(Entity entity, uint32_t flags);
		void clearEntityOverride(Entity entity, uint32_t flags);
		int reloadAllInstancesOf(const std::string& assetPath);
		std::vector<ClusterInstance*> getInstancesByAsset(const std::string& assetPath);

		// Internal registration used by SceneSerializer
		ClusterInstance& addInstance(ClusterInstance instance);

	private:
		Scene& m_scene;
		std::vector<ClusterInstance>& m_clusters;
		uint32_t m_nextClusterId = 1;
	};
} // namespace WeirdEngine
