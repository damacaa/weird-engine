#pragma once

#include "../EditorProject.h"
#include "EditorTypes.h"
#include "weird-engine/ecs/Entity.h"
#include "weird-engine/ecs/Registry.h"
#include "weird-engine/services/ServiceProvider.h"
#include "weird-engine/systems/SDFRenderSystem.h"
#include "weird-physics/Simulation2D.h"
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace WeirdEngine
{
	class Scene2D;
}

namespace WeirdEngine::Editor
{
	struct EditorContext
	{
		Scene2D* scene = nullptr;
		Registry* registry = nullptr;
		ServiceProvider* services = nullptr;
		Simulation2D* simulation2D = nullptr;
		SDFRenderSystemContext* worldRenderContext = nullptr;
		SDFRenderSystemContext* uiRenderContext = nullptr;

		EditorProject project;

		std::string currentScenePath;
		bool isSceneDirty = false;

		// Selection state
		Entity selectedEntity = INVALID_ENTITY;
		uint32_t selectedClusterId = 0;
		uint32_t selectedGroupId = 0;
		std::unordered_set<Entity> selectedEntities;
		std::unordered_set<uint32_t> selectedClusterIds;
		bool selectIndividualClusterEntities = false;

		// Hierarchy selection range
		std::vector<HierarchyItem> visibleHierarchyItems;
		HierarchyItem shiftAnchorItem{HierarchyItemType::Entity, 0};
		bool hasShiftAnchor = false;
		HierarchyItem pendingShiftTarget{HierarchyItemType::Entity, 0};
		bool pendingShiftIsCtrl = false;
		bool hasPendingShift = false;

		// Groups
		std::vector<EditorGroup> groups;
		uint32_t nextGroupId = 1;

		// Cluster rest & reset data
		std::unordered_map<uint32_t, ClusterRestData> clusterRestData;

		// Drag state
		bool isDragging = false;
		vec2 dragStartMouseWorld{0.0f, 0.0f};
		vec2 dragStartPos{0.0f, 0.0f};
		std::unordered_map<Entity, vec2> dragEntityStartPositions;
		std::unordered_map<uint32_t, vec2> dragClusterStartPositions;

		// Camera control state
		bool middleWasDown = false;

		// Grid & Snapping
		bool gridEnabled = true;
		bool gridSnap = false;
		float gridSize = 1.0f;

		// Simulation state
		bool isSimulating = false;
		bool hasSnapshot = false;
		float playGravity = -9.8f;
		float playDamping = 0.001f;

		// UI Panel Layout
		float menuBarHeight = 26.0f;
		static constexpr float LEFT_PANEL_WIDTH = 320.0f;
		static constexpr float RIGHT_PANEL_WIDTH = 340.0f;
		static constexpr float BOTTOM_BAR_HEIGHT = 38.0f;

		// Asset Browser Filter
		int assetFilterType = 0; // 0 = All, 1 = Scenes, 2 = Clusters, 3 = Shapes
		char assetSearchBuf[64] = "";
		char hierarchySearchBuf[64] = "";

		// Inspector text buffers
		char entityNameBuf[64] = "";
		char clusterNameBuf[64] = "";
		char newTagBuf[64] = "";
		uint32_t renamingGroupId = 0;
		char renameGroupBuf[128] = "";

		// Materials editor state
		bool selectMaterialsTab = false;
		int selectedMaterialSlot = 0;
		vec4 copiedMaterialColor = vec4(1.0f);
		bool hasCopiedMaterialColor = false;

		// Asset creation & tool launch state
		char newClusterNameBuf[64] = "NewCluster";
		char newShapeNameBuf[64] = "NewShape";
		bool openNewClusterModal = false;
		bool openNewShapeModal = false;

		// File dialog states
		std::mutex fileMutex;
		std::string pendingLoadScenePath;
		std::string pendingSaveScenePath;

		// Asset hot-reloading
		std::chrono::steady_clock::time_point lastAssetPollTime = std::chrono::steady_clock::now();
		std::unordered_map<std::string, std::filesystem::file_time_type> clusterAssetWriteTimes;

		// Viewport Creation Context Menu
		bool openViewportContextMenu = false;
		vec2 contextMenuWorldPos{0.0f, 0.0f};

		// Helper queries
		bool isEntitySelected(Entity e) const
		{
			return selectedEntity == e || selectedEntities.count(e) > 0;
		}

		bool isClusterSelected(uint32_t cId) const
		{
			return selectedClusterId == cId || selectedClusterIds.count(cId) > 0;
		}

		bool isGroupSelected(uint32_t gId) const
		{
			return selectedGroupId == gId;
		}

		bool hasSelection() const
		{
			return selectedEntity != INVALID_ENTITY || selectedClusterId != 0 || selectedGroupId != 0 ||
				   !selectedEntities.empty() || !selectedClusterIds.empty();
		}

		void deselectAll()
		{
			selectedEntities.clear();
			selectedClusterIds.clear();
			selectedEntity = INVALID_ENTITY;
			selectedClusterId = 0;
			selectedGroupId = 0;
			isDragging = false;
			dragEntityStartPositions.clear();
			dragClusterStartPositions.clear();
			hasShiftAnchor = false;
		}

		EditorGroup* findGroup(uint32_t gId)
		{
			for (auto& g : groups)
			{
				if (g.id == gId)
					return &g;
			}
			return nullptr;
		}

		const EditorGroup* findGroup(uint32_t gId) const
		{
			for (const auto& g : groups)
			{
				if (g.id == gId)
					return &g;
			}
			return nullptr;
		}
	};
} // namespace WeirdEngine::Editor
