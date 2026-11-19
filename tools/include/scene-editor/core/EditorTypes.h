#pragma once

#include "weird-engine/ecs/Entity.h"
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace WeirdEngine::Tools::SceneEd
{
	using glm::vec2;
	using glm::vec3;
	using glm::vec4;

	enum class HierarchyItemType
	{
		Entity,
		Cluster,
		Group
	};

	struct HierarchyItem
	{
		HierarchyItemType type = HierarchyItemType::Entity;
		uint32_t id = 0;

		bool operator==(const HierarchyItem& other) const
		{
			return type == other.type && id == other.id;
		}
	};

	struct EditorGroup
	{
		uint32_t id = 0;
		std::string name = "New Group";
		std::vector<Entity> entities;
		std::vector<uint32_t> clusterIds;
	};

	struct ClusterRestData
	{
		std::vector<vec2> restOffsets;
		std::vector<float> restRotations;
	};
} // namespace WeirdEngine::Tools::SceneEd
